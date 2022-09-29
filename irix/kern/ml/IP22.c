/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*
 * IP22 specific functions
 */

#ident "$Revision: 1.279 $"

#include <sys/types.h>
#include <ksys/xthread.h>
#include <sys/IP22.h>
#include <sys/IP22nvram.h>
#include <sys/buf.h>
#include <sys/callo.h>
#include <sys/cmn_err.h>
#include <sys/conf.h>
#include <sys/debug.h>
#include <sys/dmamap.h>
#include <sys/ds1286.h>
#include <sys/fpu.h>
#include <sys/i8254clock.h>
#include <sys/invent.h>
#include <sys/kabi.h>
#include <sys/kopt.h>
#include <sys/loaddrs.h>
#include <sys/param.h>
#include <sys/pda.h>
#include <sys/pfdat.h>
#include <sys/proc.h>
#include <ksys/vproc.h>
#include <sys/ksignal.h>
#include <sys/sbd.h>
#include <sys/schedctl.h>
#include <sys/scsi.h>
#include <sys/wd93.h>
#include <sys/sysinfo.h>
#include <sys/sysmacros.h>
#include <sys/syssgi.h>
#include <sys/systm.h>
#include <sys/ktime.h>
#include <sys/time.h>
#include <sys/var.h>
#include <sys/vdma.h>
#include <sys/vmereg.h>
#include <sys/edt.h>
#include <sys/pio.h>
#include <sys/eisa.h>
#include <sys/arcs/spb.h>
#include <sys/arcs/debug_block.h>
#include <sys/arcs/kerncb.h>
#include <sys/hal2.h>
#include <string.h>
#include <sys/wd95a_struct.h>
#include <sys/rtmon.h>
#include <sys/parity.h>
#include <sys/par.h>
#include <sys/atomic_ops.h>
#include <ksys/cacheops.h>
#include <ksys/hwg.h>
#include <sys/mc.h>

int ithreads_enabled = 0;
static int is_thd(int);
static void lclvec_thread_setup(int, int, lclvec_t *);

extern int clean_shutdown_time; 	/* mtune/kernel */
#ifdef POWER_DUMP
extern int power_button_changed_to_crash_dump;	/* mtune/kernel */
#endif

/* 
 * An important medical-equipment manufacturing customer uses IP22s.  
 * This define enables support for a feature they require:
 * On IP22s, if the "special_bus_error_handler" variable is set (by the 
 * customer's driver), a customer-provided bus error callback function 
 * is invoked, and can prevent the system from panicing.  On regular 
 * systems, the code has no affect because "special_bus_error_handler" is null.
 *
 * See also kern/os/trap.c
 *
 */
#define SPECIAL_BUS_ERROR_HANDLER

extern caddr_t map_physaddr(paddr_t);
extern void unmap_physaddr(caddr_t);

#ifdef R4600SC
extern void _r4600sc_enable_scache(void);
extern int  _r4600sc_disable_scache(void);
#endif

typedef caddr_t dmavaddr_t;
typedef paddr_t dmamaddr_t;

#ifdef _VCE_AVOIDANCE
extern int vce_avoidance;
#endif
#ifdef R4000PC
extern int pdcache_size;
#endif
#ifdef R4600
extern int two_set_pcaches;
#endif

uint_t vmeadapters = 0;

#define MAXCPU	1
#define RPSS_DIV		2
#define RPSS_DIV_CONST		(RPSS_DIV-1)

int	maxcpus = MAXCPU;

extern unsigned long cause_ip5_count;
extern unsigned long cause_ip6_count;

/* processor-dependent data area */
pdaindr_t	pdaindr[MAXCPU];

extern int	findcpufreq_raw(void);
extern int	cpu_mhz_rating(void);

void cpu_waitforreset(void);

void set_leds(int);
void set_autopoweron(void);
static int get_dayof_week(time_t year_secs);
static void set_endianness(void);
static void volumeintr(int *);
int (* volumectrl_ptr)(int, int);
int get_r4k_config(void);

/* shared by powerintr and volumeintr to pass the current volume level */
static int curlevel;

/* Global flag to reset gfx boards attached to IP22 */
int GRReset = 0;

/* software copy of write-only registers */
uint _hpc3_write1 = PAR_RESET | KBD_MS_RESET | EISA_RESET | LED_GREEN;
uint _hpc3_write2 = ETHER_AUTO_SEL | ETHER_NORM_THRESH | ETHER_UTP_STP |
	ETHER_PORT_SEL | UART0_ARC_MODE | UART1_ARC_MODE;

unsigned int mc_rev_level;
static int memconfig_0, memconfig_1;

/*
 * Processor interrupt table
 */
extern	void buserror_intr(struct eframe_s *);
extern	void clock(struct eframe_s *);
extern	void ackkgclock(void);
static	void lcl1_intr(struct eframe_s *);
static	void lcl0_intr(struct eframe_s *);
extern	void timein(void);
extern	void r4kcount_intr(struct eframe_s *);
extern	void load_nvram_tab(void);
static	void power_bell(void);
static	void flash_led(int);

/* Keep poker for soft clocks away from timein().
 */
#if DEBUG
unsigned int softpoke;
#endif
/*ARGSUSED*/
void
pokesoftclk(struct eframe_s *ep)
{
#if DEBUG
	softpoke++;
#endif
	siroff(CAUSE_SW2);
	return;
}

typedef void (*intvec_func_t)(struct eframe_s *);

struct intvec_s {
	intvec_func_t	isr;		/* interrupt service routine */
	uint		msk;		/* spl level for isr */
	uint		bit;		/* corresponding bit in sr */
	int		fil;		/* make sizeof this struct 2^n */
};

static struct intvec_s c0vec_tbl[] = {
	0,	    0,			0,		0,  /* (1 based) */
	(intvec_func_t)
	timein,     SR_IMASK5|SR_IEC,   SR_IBIT1 >> 8,	0,  /* softint 1 */
	pokesoftclk,SR_IMASK5|SR_IEC,	SR_IBIT2 >> 8,	0,  /* softint 2 */
	(intvec_func_t)
	lcl0_intr, SR_IMASK5|SR_IEC,	SR_IBIT3 >> 8, 0,   /* hardint 3 */
	(intvec_func_t)
	lcl1_intr, SR_IMASK5|SR_IEC,	SR_IBIT4 >> 8, 0,   /* hardint 4 */
	clock,	    SR_IMASK5|SR_IEC,	SR_IBIT5 >> 8,	0,  /* hardint 5 */
	(intvec_func_t)
	ackkgclock, SR_IMASK6|SR_IEC,	SR_IBIT6 >> 8,	0,  /* hardint 6 */
	(intvec_func_t)
	buserror_intr, SR_IMASK7|SR_IEC,SR_IBIT7 >> 8,	0,  /* hardint 7 */
	r4kcount_intr, SR_IMASK8|SR_IEC,SR_IBIT8 >> 8,	0,  /* hardint 8 */
};


/*
 * Local interrupt table
 *
 * The isr field is the address of the interrupt service routine.  It may
 * be changed dynamically by use of the setlclvector() call.
 *
 * The 'bit' value is the bit in the local interrupt status register that
 * causes the interrupt.  It is in the table to save a few instructions
 * in lcl_intr().
 */
static	lcl_intr_func_t lcl_stray;
static  lcl_intr_func_t hpcdmastray;
static  lcl_intr_func_t hpcdmastray_scsi;
static	lcl_intr_func_t ip22_gio0_intr;
static	lcl_intr_func_t ip22_gio1_intr;
static	lcl_intr_func_t ip22_gio2_intr;
static  lcl_intr_func_t lcl2_intr;
#if 0
static  lcl_intr_func_t lcl3_intr;
#endif
static	lcl_intr_func_t powerfail;
static  lcl_intr_func_t powerintr;

lcl_intr_func_t hpcdma_intr;

/* These arrays must be left in order, and contiguous to each other,
 * since we do some address calculations on them.
 */
lclvec_t lcl0vec_tbl[] = {
	{ 0,		0 } ,		/* table is 1 based */
	{ lcl_stray,	1, 0x01 },
	{ lcl_stray,	2, 0x02 },
	{ lcl_stray,	3, 0x04 },
	{ lcl_stray,	4, 0x08 },
	{ lcl_stray,	5, 0x10 },
	{ lcl_stray,	6, 0x20 },
	{ lcl_stray,	7, 0x40 },
	{ lcl_stray,	8, 0x80 }
	};


lclvec_t lcl1vec_tbl[] = {
	{ 0,		0 },		/* table is 1 based */
	{ lcl_stray,	8+1, 0x01 },
	{ lcl_stray,	8+2, 0x02 },
	{ lcl_stray,	8+3, 0x04 },
	{ lcl_stray,	8+4, 0x08 },
	{ lcl_stray,	8+5, 0x10 },
	{ lcl_stray,	8+6, 0x20 },
	{ lcl_stray,	8+7, 0x40 },
	{ lcl_stray,	8+8, 0x80 }
	};

lclvec_t lcl2vec_tbl[] = {
	{ 0,		0 },		/* table is 1 based */
	{ lcl_stray,	16+1, 0x01 },
	{ lcl_stray,	16+2, 0x02 },
	{ lcl_stray,	16+3, 0x04 },
	{ lcl_stray,	16+4, 0x08 },
	{ lcl_stray,	16+5, 0x10 },
	{ lcl_stray,	16+6, 0x20 },
	{ lcl_stray,	16+7, 0x40 },
	{ lcl_stray,	16+8, 0x80 }
	};

static lclvec_t hpcdmavec_tbl[] = {
	{ 0,			0 },		/* table is 1 based */
	{ hpcdmastray,		1, 0x01 },
	{ hpcdmastray,		2, 0x02 },
	{ hpcdmastray,		3, 0x04 },
	{ hpcdmastray,		4, 0x08 },
	{ hpcdmastray,		5, 0x10 },
	{ hpcdmastray,		6, 0x20 },
	{ hpcdmastray,		7, 0x40 },
	{ hpcdmastray,		8, 0x80 },
	{ hpcdmastray_scsi,	9, 0x100 },
	{ hpcdmastray_scsi,	10,0x200 }
	};
#define HPCDMAVECTBL_SLOTS (sizeof(hpcdmavec_tbl) / sizeof(lclvec_t) - 1)

#define GIOLEVELS	3 /* GIO 0 (FIFO), GIO 1 (GE), GIO 2 (VR) */
#define GIOSLOTS	3 /* slot 0, slot 1 (IP22B/IP24 only), gfx */

struct giovec_s {
	lcl_intr_func_t *isr;
	__psint_t arg;
};

static struct giovec_s giovec_tbl[GIOSLOTS][GIOLEVELS];

scuzzy_t scuzzy[] = {
        {
		(u_char *)PHYS_TO_K1(SCSI0A_ADDR),
		(u_char *)PHYS_TO_K1(SCSI0D_ADDR),
                (uint *)PHYS_TO_K1(SCSI0_CTRL_ADDR),
                (uint *)PHYS_TO_K1(SCSI0_BC_ADDR),
                (uint *)PHYS_TO_K1(SCSI0_CBP_ADDR),
                (uint *)PHYS_TO_K1(SCSI0_NBDP_ADDR),
                (uint *)PHYS_TO_K1(HPC3_SCSI_DMACFG0),
                (uint *)PHYS_TO_K1(HPC3_SCSI_PIOCFG0),
		D_PROMSYNC|D_MAPRETAIN,
		0x80,
		SCSI_RESET, SCSI_FLUSH, SCSI_STARTDMA,
		SCSI_STARTDMA, SCSI_STARTDMA | SCDIROUT,
		HPC3_CBP_OFFSET, HPC3_BCNT_OFFSET, HPC3_NBP_OFFSET,
		VECTOR_SCSI
        },
        {
		(u_char *)PHYS_TO_K1(SCSI1A_ADDR),
		(u_char *)PHYS_TO_K1(SCSI1D_ADDR),
                (uint *)PHYS_TO_K1(SCSI1_CTRL_ADDR),
                (uint *)PHYS_TO_K1(SCSI1_BC_ADDR),
                (uint *)PHYS_TO_K1(SCSI1_CBP_ADDR),
                (uint *)PHYS_TO_K1(SCSI1_NBDP_ADDR),
                (uint *)PHYS_TO_K1(HPC3_SCSI_DMACFG1),
                (uint *)PHYS_TO_K1(HPC3_SCSI_PIOCFG1),
		D_PROMSYNC|D_MAPRETAIN,
		0x80,
		SCSI_RESET, SCSI_FLUSH, SCSI_STARTDMA,
		SCSI_STARTDMA, SCSI_STARTDMA | SCDIROUT,
		HPC3_CBP_OFFSET, HPC3_BCNT_OFFSET, HPC3_NBP_OFFSET,
		VECTOR_SCSI1
        },
        {
                (u_char *)PHYS_TO_K1(SCSI2A_ADDR),
                (u_char *)PHYS_TO_K1(SCSI2D_ADDR),
                (uint *)PHYS_TO_K1(SCSI2_CTRL_ADDR),
                (uint *)PHYS_TO_K1(SCSI2_BC_ADDR),
                (uint *)PHYS_TO_K1(SCSI2_CBP_ADDR),
                (uint *)PHYS_TO_K1(SCSI2_NBDP_ADDR),
                (uint *)PHYS_TO_K1(HPC1_SCSI_DMACFG2),
                (uint *)PHYS_TO_K1(HPC1_SCSI_PIOCFG2),
		D_PROMSYNC|D_MAPRETAIN,
                0x80,
		HPC1_SCSI_RESET, HPC1_SCSI_FLUSH, HPC1_SCSI_STARTDMA,
		HPC1_SCSI_STARTDMA | HPC1_SCSI_TO_MEM, HPC1_SCSI_STARTDMA,
		HPC1_CBP_OFFSET, HPC1_BCNT_OFFSET, HPC1_NBP_OFFSET,
		GIO_INTERRUPT_0
        },
        {
                (u_char *)PHYS_TO_K1(SCSI3A_ADDR),
                (u_char *)PHYS_TO_K1(SCSI3D_ADDR),
                (uint *)PHYS_TO_K1(SCSI3_CTRL_ADDR),
                (uint *)PHYS_TO_K1(SCSI3_BC_ADDR),
                (uint *)PHYS_TO_K1(SCSI3_CBP_ADDR),
                (uint *)PHYS_TO_K1(SCSI3_NBDP_ADDR),
                (uint *)PHYS_TO_K1(HPC1_SCSI_DMACFG3),
                (uint *)PHYS_TO_K1(HPC1_SCSI_PIOCFG3),
		D_PROMSYNC|D_MAPRETAIN,
                0x80,
		HPC1_SCSI_RESET, HPC1_SCSI_FLUSH, HPC1_SCSI_STARTDMA,
		HPC1_SCSI_STARTDMA | HPC1_SCSI_TO_MEM, HPC1_SCSI_STARTDMA,
		HPC1_CBP_OFFSET, HPC1_BCNT_OFFSET, HPC1_NBP_OFFSET,
		GIO_INTERRUPT_1
        },
        {
		(u_char *)PHYS_TO_K1(SCSI4A_ADDR),
		(u_char *)PHYS_TO_K1(SCSI4D_ADDR),
		(uint *)PHYS_TO_K1(SCSI4_CTRL_ADDR),
		(uint *)PHYS_TO_K1(SCSI4_BC_ADDR),
		(uint *)PHYS_TO_K1(SCSI4_CBP_ADDR),
		(uint *)PHYS_TO_K1(SCSI4_NBDP_ADDR),
		(uint *)PHYS_TO_K1(HPC31_SCSI_DMACFG0),
		(uint *)PHYS_TO_K1(HPC31_SCSI_PIOCFG0),
		0,
		0x80,
		SCSI_RESET, SCSI_FLUSH, SCSI_STARTDMA,
		SCSI_STARTDMA, SCSI_STARTDMA | SCDIROUT,
		HPC3_CBP_OFFSET, HPC3_BCNT_OFFSET, HPC3_NBP_OFFSET,
		VECTOR_GIO0
        },
        {
		(u_char *)PHYS_TO_K1(SCSI5A_ADDR),
		(u_char *)PHYS_TO_K1(SCSI5D_ADDR),
		(uint *)PHYS_TO_K1(SCSI5_CTRL_ADDR),
		(uint *)PHYS_TO_K1(SCSI5_BC_ADDR),
		(uint *)PHYS_TO_K1(SCSI5_CBP_ADDR),
		(uint *)PHYS_TO_K1(SCSI5_NBDP_ADDR),
		(uint *)PHYS_TO_K1(HPC31_SCSI_DMACFG1),
		(uint *)PHYS_TO_K1(HPC31_SCSI_PIOCFG1),
		0,
		0x80,
		SCSI_RESET, SCSI_FLUSH, SCSI_STARTDMA,
		SCSI_STARTDMA, SCSI_STARTDMA | SCDIROUT,
		HPC3_CBP_OFFSET, HPC3_BCNT_OFFSET, HPC3_NBP_OFFSET,
		VECTOR_GIO0
        }
};

/*	used to differentiate between GIO and HPC3 SCSI devices - Indy only */
#define GIOSCSI	2
#define GIOSCSIMAX 3

/*	used to sanity check adap values passed in, etc */
#define NSCUZZY (sizeof(scuzzy)/sizeof(scuzzy_t))

/* HPC3 can DMA past 256 Mbytes, unlike HPC1.  It has a different memory
 * map than some ASD machines though, so DMA'able K0/K1 memory goes to
 * 384 Mbytes, not 256.  DMA'able memory is actually all of memory,
 * but for now this is tied up with VM_DIRECT memory allocation.
*/
#define MAXDMAPAGE 0x18000

#define PROBE_SPACE(X)	PHYS_TO_K1(X)

/* table of probeable kmem addresses */
#define HPC_HOLLY_1_LIO 	PHYS_TO_K1(0x1FB001C0)
#define HPC_HOLLY_2_LIO 	PHYS_TO_K1(0x1F9801C0)
#define HPC3_NOT_HPC1		PHYS_TO_K1(0x1FB02000)
struct kmem_ioaddr kmem_ioaddr[] = {
        { PROBE_SPACE(LIO_ADDR), (LIO_GFX_SIZE + LIO_GIO_SIZE) },
        { PROBE_SPACE(HPC3_SYS_ID), sizeof (int) },
        { PROBE_SPACE(HAL2_ADDR), (HAL2_REV - HAL2_ADDR + sizeof(int))},
	/* Indy only -- must be last since mlreset() zeros it on Indigo2 */
        { PROBE_SPACE(HPC_HOLLY_1_LIO), sizeof (int) },
        { PROBE_SPACE(HPC_HOLLY_2_LIO), sizeof (int) },
	{ PROBE_SPACE(HPC3_NOT_HPC1), sizeof(int) },
#define HPC_1_KMEM_PROBE 3
#define HPC_2_KMEM_PROBE 4
#define HPC_3_KMEM_PROBE 5
	{ 0, 0 },
};

short	cputype = 22;   /* integer value for cpu (i.e. xx in IPxx) */

static uint sys_id;	/* initialized by init_sysid */

/* I/O and graphics DMA burst & delay periods;  The value is
 * register_value * GIO clock period
 */
/*
 * due to a bug in the MC, reading of GIO64_ARB/LB_TIME/CPU_TIME registers
 * sometimes returns corrupted values.  therefore, writing to these
 * registers should always be done through the software copy
 */
#define	GIO_CYCLE	30		/* assume 33 MHZ clock for now */
unsigned short dma_burst = 8000 / GIO_CYCLE;	/* 8 usecs */
unsigned short dma_delay = 700 / GIO_CYCLE;	/* .7 usec */

u_short i_dma_burst, i_dma_delay;

/* default values for different revs of base board */
#define GIO64_ARB_003 (	GIO64_ARB_HPC_SIZE_64	| GIO64_ARB_HPC_EXP_SIZE_64 |\
			GIO64_ARB_1_GIO		| GIO64_ARB_EXP0_PIPED |\
			GIO64_ARB_EXP1_MST	| GIO64_ARB_EXP1_RT )
#define GIO64_ARB_004 ( GIO64_ARB_HPC_SIZE_64	| GIO64_ARB_HPC_EXP_SIZE_64 |\
			GIO64_ARB_1_GIO		| GIO64_ARB_EXP0_PIPED |\
			GIO64_ARB_EXP1_PIPED	| GIO64_ARB_EISA_MST )
#define GIO64_ARB_GNS ( GIO64_ARB_HPC_SIZE_64	| GIO64_ARB_1_GIO |\
			GIO64_ARB_EISA_SIZE	| GIO64_ARB_EISA_MST )
u_int gio64_arb_reg;

#define ISXDIGIT(c) ((('a'<=(c))&&((c)<='f'))||(('0'<=(c))&&((c)<='9')))

#define HEXVAL(c) ((('0'<=(c))&&((c)<='9'))? ((c)-'0')  : ((c)-'a'+10))
char eaddr[6];

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
 * initialize the serial number.  Called from mlreset.
 */
void
init_sysid(void)
{
	char *cp; 

	cp = (char *)arcs_getenv("eaddr");
	bcopy (etoh(cp), eaddr, 6);

	sys_id = (eaddr[2] << 24) | (eaddr[3] << 16) |
		 (eaddr[4] << 8) | eaddr[5];
	sprintf (arg_eaddr, "%x%x:%x%x:%x%x:%x%x:%x%x:%x%x",
	    (eaddr[0]>>4)&0xf, eaddr[0]&0xf,
	    (eaddr[1]>>4)&0xf, eaddr[1]&0xf,
	    (eaddr[2]>>4)&0xf, eaddr[2]&0xf,
	    (__psunsigned_t)(eaddr[3]>>4)&0xf, (__psunsigned_t)eaddr[3]&0xf,
	    (__psunsigned_t)(eaddr[4]>>4)&0xf, (__psunsigned_t)eaddr[4]&0xf,
	    (__psunsigned_t)(eaddr[5]>>4)&0xf, (__psunsigned_t)eaddr[5]&0xf);
}

/*
 *	pass back the serial number associated with the system
 *  ID prom. always returns zero.
 */
int
getsysid(char *hostident)
{
	/*
	 * serial number is only 4 bytes long on IP22.  Left justify
	 * in memory passed in...  Zero out the balance.
	 */

	*(uint *) hostident = sys_id;
	bzero(hostident + 4, MAXSYSIDSIZE - 4);

	return 0;
}

#define       IS_R4400(maj,min)       (!strcmp("R4400", cpu_rev_find(private.p_cputype_word, &maj, &min)))
#define       IS_250()                (cpu_mhz_rating() > 240)
#define       IS_DIV4()               ((get_r4k_config() & CONFIG_EC) == (2 << 28))

#define       IS_PM5(maj,min)         (IS_R4400(maj,min) && IS_250() && IS_DIV4())

/* Indigo2/Indy boolean routines for machine dependant routines.
 */
#ifdef R4600
int is_r4600_flag = 0;
#endif
int is_fullhouse_flag;
int is_ioc1_flag = -1;
int r4000_clock_war = -1;
static void
setup_ip22_flags(void)
{
	uint id = *(volatile uint *)PHYS_TO_K1(HPC3_SYS_ID);
#if R4000
	extern int get_cpu_irr(void);
	extern int has_250;	/* flag checked in hook_exc.c */
#endif

	is_fullhouse_flag = ((id & BOARD_ID_MASK) == BOARD_IP22);

	/* This probably should be >= CHIP_IOC1, but I'll let the next
	 * IOC person do this.
	 */
	if ((id & CHIP_REV_MASK) == CHIP_IOC1) {
		/* IP22 boards with IOC also have IOC1.2 (aka IOC 2)
		 * P1 Guinness boards (ID 2) all have IOC1.2
		 */
		if ( ((id & BOARD_ID_MASK) == BOARD_IP22) ||
		     (((id & BOARD_REV_MASK) >> BOARD_REV_SHIFT) >= 2) ) {
			is_ioc1_flag = 2;
		} else {
			/* Must be an old IOC1.1 (Guinness P0) */
			is_ioc1_flag = 1;
		}
	} else {
		/* Pre-006 Fullhouse system */
		is_ioc1_flag = 0;
	}
#ifdef R4600
	if (! (is_ioc1_flag &&
	       ! is_r4600_flag))
		r4000_clock_war = 0;
	else
#endif
		r4000_clock_war = is_ioc1_flag;
#if R4000
	/* Note if this is a R4400 250MHz module */
	if (((get_cpu_irr() >> C0_IMPSHIFT) == C0_IMP_R4400) &&
		((findcpufreq() * 2) == 250))
		has_250++;
#endif
}

/*
 * return 1 if fullhouse system, 0 if guinness system
 */
int
is_fullhouse(void)
{
	return(is_fullhouse_flag);
}

/*
 * return revision (> 0) if IOC1 chip, 0 if not IOC.
 */
int
is_ioc1(void)
{
	return(is_ioc1_flag);
}

/* Local IP22.c optimizations
 */
#define is_fullhouse()	is_fullhouse_flag
#define is_ioc1()	is_ioc1_flag

/* 
 *	get memory configuration word for a bank
 *              - bank better be between 0 and 3 inclusive
 */
static uint
get_mem_conf(int bank)
{
    uint memconfig;

    memconfig = (bank <= 1) ? 
		*(volatile uint *)PHYS_TO_K1(MEMCFG0) :
                *(volatile uint *)PHYS_TO_K1(MEMCFG1) ;
    memconfig >>= 16 * (~bank & 1);     /* shift banks 0, 2 */
    return memconfig & 0xffff;
}       /* get_mem_conf */

/*
 * bank_addrlo - determine first address of a memory bank
 */
static int
bank_addrlo(int bank)
{
    return (int)((get_mem_conf(bank) & MEMCFG_ADDR_MASK) << 
	(mc_rev_level >= 5 ? 24 : 22));
}       /* bank_addrlo */

/*
 * banksz - determine the size of a memory bank
 */
static int
banksz(int bank)
{
    uint memconfig = get_mem_conf(bank);

    /* weed out bad module sizes */
    if (!memconfig)
        return 0;
    else
        return (int)(((memconfig & MEMCFG_DRAM_MASK) + 0x0100) << 
		(mc_rev_level >= 5 ? 16 : 14));
}       /* banksz */

/*
 * addr_to_bank - determine which physical memory bank contains an address
 *              returns 0-3 or -1 for bad address
 */
int
addr_to_bank(uint addr)
{
    int bank;
    uint size;

    for (bank = 0; bank < MAX_MEM_BANKS; ++bank) {
        if (!(size = banksz(bank))) /* bad memory module if size == 0 */
            continue;

        if (addr >= bank_addrlo(bank) && addr < bank_addrlo(bank) + size)
            return (bank);
    }
    return (-1);        /* address not found */
}   /* addr_to_bank */

static pgno_t memsize = 0;	/* total ram in clicks */
static pgno_t seg0_maxmem;
static pgno_t seg1_maxmem; 

/*
 * szmem - size main memory
 * Returns # physical pages of memory
 */
/* ARGSUSED */
pfn_t
szmem(pfn_t fpage, pfn_t maxpmem)
{
    extern pfn_t low_maxclick;
    int bank;
    int bank_size;

    if (!memsize) {
	for (bank = 0; bank < MAX_MEM_BANKS; ++bank) {
	    bank_size = banksz(bank);
	    memsize += bank_size;		/* sum up bank sizes */
	    if (bank_size) {
		if (bank_addrlo(bank) >= SEG1_BASE)
			seg1_maxmem += bank_size;
		else
			seg0_maxmem += bank_size;
	    }
	}
	memsize = btoct(memsize);			/* get pages */
	seg0_maxmem = btoct(seg0_maxmem);
	seg1_maxmem = btoct(seg1_maxmem);

	if (maxpmem) {
	    memsize = MIN(memsize, maxpmem);
	    seg0_maxmem = MIN(memsize, seg0_maxmem);
	    seg1_maxmem = MIN(memsize - seg0_maxmem, seg1_maxmem);
	}
    }

    low_maxclick = (pfn_t)(btoct(SEG0_BASE) + seg0_maxmem);

    return memsize;
}

#ifndef _MEM_PARITY_WAR
static
#endif /* _MEM_PARITY_WAR */
int
is_in_pfdat(pgno_t pfn)
{
	pgno_t min_pfn = btoct(kpbase);

	return((pfn >= min_pfn &&
		pfn < (btoct(SEG0_BASE) + seg0_maxmem)) ||
	       (pfn >= btoct(SEG1_BASE) &&
	        pfn < (btoct(SEG1_BASE) + seg1_maxmem)));
}

#ifdef _MEM_PARITY_WAR
int
is_in_main_memory(pgno_t pfn)
{
	return((pfn >= btoct(SEG0_BASE) &&
		pfn < (btoct(SEG0_BASE) + seg0_maxmem)) ||
	       (pfn >= btoct(SEG1_BASE) &&
		pfn < (btoct(SEG1_BASE) + seg1_maxmem)));
}
#endif /* _MEM_PARITY_WAR */


#ifdef DEBUG
int mapflushcoll;	/* K2 address collision counter */
int mapflushdepth;	/* K2 address collision maximum depth */
#endif

static void
mapflush(void *addr, unsigned int len, unsigned int pfn, unsigned int flags)
{
    void *kvaddr;
    uint lastpgi = 0;
    int s;
    int current_color;
    static int *pginuse;

    if (!private.p_cacheop_pages) {
	private.p_cacheop_pages = kvalloc(cachecolormask+1, VM_VACOLOR, 0);
	pginuse = (int *)kmem_zalloc(sizeof(int) * (cachecolormask+1), 0);
	ASSERT(private.p_cacheop_pages != NULL && pginuse != NULL);
    }
#ifdef _VCE_AVOIDANCE
    if (vce_avoidance && is_in_pfdat(pfn))
	current_color = pfd_to_vcolor(pfntopfdat(pfn));
    else
	current_color = -1;
    if (current_color == -1)
	current_color = colorof(addr);
#else
    current_color = colorof(addr);
#endif

    kvaddr = (void *)((__psunsigned_t)private.p_cacheop_pages +
		  (NBPP * current_color) + poff(addr));


    s = splhi();
    if (pginuse[current_color]) {
#ifdef DEBUG
	mapflushcoll++;
	if (pginuse[current_color] > mapflushdepth)
	    mapflushdepth = pginuse[current_color];
#endif
	/* save last mapping */
	lastpgi = kvtokptbl(kvaddr)->pgi;
	ASSERT(lastpgi && lastpgi != mkpde(PG_G, 0));
	unmaptlb(0, btoct(kvaddr));
    }
    pg_setpgi(kvtokptbl(kvaddr),
	      mkpde(PG_VR|PG_G|PG_SV|PG_M|pte_cachebits(), pfn));
    pginuse[current_color]++;

    if (flags & CACH_ICACHE) {
	__icache_inval(kvaddr, len);
    } else {
	if ((flags & (CACH_INVAL|CACH_WBACK)) == (CACH_INVAL|CACH_WBACK))
		__dcache_wb_inval(kvaddr, len);
	else if (flags & CACH_INVAL)
		__dcache_inval(kvaddr, len);
	else if (flags & CACH_WBACK)
		__dcache_wb(kvaddr, len);
    }

    unmaptlb(0, btoct(kvaddr));
    if (lastpgi) {
	/* restore last mapping */
	ASSERT(lastpgi != mkpde(PG_G, 0));
	pg_setpgi(kvtokptbl(kvaddr), lastpgi);
	tlbdropin(0, kvaddr, kvtokptbl(kvaddr)->pte);
    } else
	pg_setpgi(kvtokptbl(kvaddr), mkpde(PG_G, 0));
    pginuse[current_color]--;

    splx(s);
}

/*
 * On the IP22, the secondary cache is a unified i/d cache. The
 * r4000 caches must maintain the property that the primary caches
 * are a subset of the secondary cache - any line that is present in
 * the primary must also be present in the secondary. A line can be
 * dirty in the primary and clean in the secondary, of coarse, but
 * the subset property must be maintained. Therefore, if we invalidate
 * lines in the secondary, we must also invalidate lines in both the
 * primary i and d caches. So when we use the 'index invalidate' or
 * 'index writeback invalidate' cache operations, those operations
 * must be targetted at both the primary caches.
 *
 * When we use the 'hit' operations, the operation can be targetted
 * selectively at the primary i-cache or the d-cache.
 */

void
clean_icache(void *addr, unsigned int len, unsigned int pfn, unsigned int flags)
{
	char *kvaddr;

	/* Low level routines can't handle length of zero */
	if (len == 0)
		return;

	ASSERT(flags & CACH_OPMASK);
	ASSERT(!IS_KSEG1(addr));	/* catch PHYS_TO_K0 on high addrs */

	/* Note that this routine is called with either a K0 address and
	 * an arbitrary length OR higher level routines will break request
	 * into (at most) page size requests with non-K0 addresses.
	 */
	if ((flags & CACH_NOADDR) && IS_KSEG0(addr)) {
		__cache_wb_inval(addr, len);
		return;
	}
	ASSERT(IS_KSEG0(addr) || (btoct(addr)==btoct((__psint_t)addr+len-1)));

	/*
	 * Remap the page to flush if
	 * 	the page is mapped and vce_avoidance is enabled, or
	 *	the page is in high memory
	 */

	if (IS_KUSEG(addr) || (flags & CACH_NOADDR)) {
		ASSERT(pfn);
		if (
#ifdef _VCE_AVOIDANCE
		(vce_avoidance && is_in_pfdat(pfn)) ||
#endif
		    (pfn >= btoct(SEG1_BASE))) {

		    mapflush(addr, len, pfn, flags & ~CACH_DCACHE);
		    return;
		} else
		    kvaddr = (char *)(PHYS_TO_K0(ctob(pfn)) | poff(addr));
	} else
		kvaddr = addr;

	__icache_inval(kvaddr, len);
}

void
clean_dcache(void *addr, unsigned int len, unsigned int pfn, unsigned int flags)
{
	char *kvaddr;

	/* Low level routines can't handle length of zero */
	if (len == 0)
		return;

	ASSERT(flags & CACH_OPMASK);
	ASSERT(!IS_KSEG1(addr));	/* catch PHYS_TO_K0 on high addrs */

	/* Note that this routine is called with either a K0 address and
	 * an arbitrary length OR higher level routines will break request
	 * into (at most) page size requests with non-K0 addresses.
	 */
	if ((flags & CACH_NOADDR) && IS_KSEG0(addr)) {
		__cache_wb_inval(addr, len);
		return;
	}
	ASSERT(IS_KSEG0(addr) || (btoct(addr)==btoct((__psint_t)addr+len-1)));

	/*
	 * Remap the page to flush if
	 * 	the page is mapped and vce_avoidance is enabled, or
	 *	the page is in high memory
	 */

	if (IS_KUSEG(addr) || (flags & CACH_NOADDR)) {
		ASSERT(pfn);
		if (
#ifdef _VCE_AVOIDANCE
		(vce_avoidance && is_in_pfdat(pfn)) ||
#endif
		    (pfn >= btoct(SEG1_BASE))) {

		    mapflush(addr, len, pfn, flags & ~CACH_ICACHE);
		    return;
		} else
		    kvaddr = (char *)(PHYS_TO_K0(ctob(pfn)) | poff(addr));
	} else
		kvaddr = addr;

	if ((flags & (CACH_INVAL|CACH_WBACK)) == (CACH_INVAL|CACH_WBACK)) {
		__dcache_wb_inval(kvaddr, len);
	} else if (flags & CACH_INVAL) {
		__dcache_inval(kvaddr, len);
	} else if (flags & CACH_WBACK) {
		__dcache_wb(kvaddr, len);
	}
}

/*
 * getfirstclick - returns pfn of first page of ram
 */
pfn_t
pmem_getfirstclick(void)
{
	return btoc(SEG0_BASE);
}

/*
 * getfirstfree - returns pfn of first page of unused ram.  This allows
 *		  prom code on some platforms to put things in low memory
 *		  that the kernel must skip over when its starting up.
 *		  This isn't used on IP22.
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
	ASSERT(node == 0);
	/* 
	 * szmem must be called before getmaxclick because of
	 * its depencency on maxpmem
	 */
	ASSERT(memsize);

	if (seg1_maxmem)
		return btoc(SEG1_BASE) + seg1_maxmem - 1;

	return btoc(SEG0_BASE) + memsize - 1;
}

/*
 * setupbadmem - mark the hole in memory between seg 0 and seg 1.
 *		 return count of non-existent pages.
 */

/*ARGSUSED3*/
pfn_t
setupbadmem(pfd_t *pfd, pfn_t first, pfn_t last)
{
	register pgno_t	hole_size;
	register pgno_t npgs;

	if (!seg1_maxmem)
		return 0;

	pfd += btoc(SEG0_BASE) + seg0_maxmem - first;
	hole_size = btoc(SEG1_BASE) - (btoc(SEG0_BASE) + seg0_maxmem); 

	for (npgs = hole_size; npgs; pfd++, npgs--)
		PG_MARKHOLE(pfd);

	return hole_size;
}

void
lclvec_init(void)
{
	int i;
	for (i=0; i<8; i++) {
		lcl0vec_tbl[i+1].lcl_flags=is_thd( 0+i) ? THD_ISTHREAD|0 : 0;
		lcl1vec_tbl[i+1].lcl_flags=is_thd( 8+i) ? THD_ISTHREAD|1 : 1;
		lcl2vec_tbl[i+1].lcl_flags=is_thd(16+i) ? THD_ISTHREAD|2 : 2;
	}
}

unsigned long hpc3_int_addr;

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
	extern int get_cpu_irr(void);
	extern int reboot_on_panic;
	extern uint cachecolormask;
	extern int softpowerup_ok;
	extern int cachewrback;
	unsigned int cpuctrl1tmp;
	volatile uint *cpuctrl0 = (volatile uint *)PHYS_TO_K1(CPUCTRL0);
#ifdef TRITON
	extern int _triton_use_invall;
#endif /* TRITON */
#ifdef R4600
	rev_id_t ri;

	ri.ri_uint = get_cpu_irr();
	switch (ri.ri_imp) {
#ifdef TRITON
	case C0_IMP_TRITON:
		_triton_use_invall = is_enabled(arg_triton_invall);
		/* FALL THROUGH */
#endif /* TRITON */
	case C0_IMP_R4700:
	case C0_IMP_R4600:
		is_r4600_flag = 1;
		break;
	default:
		break;
	}
#endif

	/*
	** just to be safe, clear the parity error register
	** This is done because if any errors are left in the register,
	** when a DBE occurs we will think it is due to a parity error.
	*/
	*(volatile uint *)PHYS_TO_K1( CPU_ERR_STAT ) = 0;
	*(volatile uint *)PHYS_TO_K1( GIO_ERR_STAT ) = 0;
	flushbus();

	/*
	 * SPECIAL_GIO_RESET is only meaningful on the specially-modified
	 * IP22 systems used by an important medical equipment-manufacturing
	 * customer; it controls reset of the special 3rd GIO slot.  It
	 * is harmless on normal IP22's: it corresponds to an unused bit in the
	 * register. -- wtw
	 */
	if (is_fullhouse())
	    _hpc3_write1 |= SPECIAL_GIO_RESET;	       /* unreset 3rd slot */
	*(volatile uint *)PHYS_TO_K1( HPC3_WRITE1 ) = _hpc3_write1;
	*(volatile uint *)PHYS_TO_K1( HPC3_WRITE2 ) = _hpc3_write2;
	flushbus();

	/* HPC3_EXT_IO_ADDR is 16 bits wide */
	*(volatile int *)PHYS_TO_K1(HPC3_PBUS_CFG_ADDR(6)) |= HPC3_CFGPIO_DS_16;

	/*
	 * Set RPSS divider for increment by 2, the fastest rate at which
	 * the register works  (also see comment for query_cyclecntr).
	 */
	*(volatile uint *)PHYS_TO_K1( RPSS_DIVIDER ) = (1 << 8)|RPSS_DIV_CONST;

	/* enable all parity checking except SYSAD bus */
#ifdef R4600
	if (is_r4600_flag) {
		*cpuctrl0 |= CPUCTRL0_GPR|CPUCTRL0_MPR|
			CPUCTRL0_R4K_CHK_PAR_N;
		if (enable_orion_parity) {
			flushbus();
			*cpuctrl0 |= CPUCTRL0_CPR|CPUCTRL0_CMD_PAR;
		}
	} else
#endif
	*cpuctrl0 |= CPUCTRL0_GPR|CPUCTRL0_MPR|CPUCTRL0_CPR|CPUCTRL0_CMD_PAR|
		CPUCTRL0_R4K_CHK_PAR_N;
	flushbus();

	/* Rev A MC unsupported in IP22, but global still needed in gr2 code.
	 */
	mc_rev_level = *(volatile uint *)PHYS_TO_K1(SYSID) &
		SYSID_CHIP_REV_MASK;

	/*
	 * set the MC write buffer depth
	 * the effective depth is roughly 17 minus this value (i.e. 6)
	 * NOTE: don't make the write buffer deeper unless you
         * understand what this does to GFXDELAY
	 */
	cpuctrl1tmp = *((volatile unsigned int *)PHYS_TO_K1(CPUCTRL1)) & 0xFFFFFFF0;
	*((volatile unsigned int *)PHYS_TO_K1(CPUCTRL1)) = (cpuctrl1tmp | 0xD);
	flushbus();

	/* initialize is_fullhouse() and is_ioc1().
	 */
	setup_ip22_flags();

	if (is_fullhouse())
		gio64_arb_reg = (board_rev() >= 2) ? GIO64_ARB_004 :
						     GIO64_ARB_003 ;
	else
		gio64_arb_reg = GIO64_ARB_GNS;

	/* Do not have second/third gio slot hpc1.5 on Indigo2
	 */
	if (is_fullhouse()) {
		kmem_ioaddr[HPC_1_KMEM_PROBE].v_base = NULL;
		kmem_ioaddr[HPC_1_KMEM_PROBE].v_length = 0;
		kmem_ioaddr[HPC_2_KMEM_PROBE].v_base = NULL;
		kmem_ioaddr[HPC_2_KMEM_PROBE].v_length = 0;
		kmem_ioaddr[HPC_3_KMEM_PROBE].v_base = NULL;
		kmem_ioaddr[HPC_3_KMEM_PROBE].v_length = 0;
	}

	*(volatile u_int *)PHYS_TO_K1(GIO64_ARB) = gio64_arb_reg;
	flushbus();

	/* If kdebug has not been set, dbgmon hasn't been loaded
	 * and we should turn kdebug off.
	 */
	if (kdebug == -1) 
	    kdebug = 0;
	
	/* set initial interrupt vectors */
	*K1_LIO_0_MASK_ADDR = 0;
	*K1_LIO_1_MASK_ADDR = 0;
	*K1_LIO_2_MASK_ADDR = 0;
	*K1_LIO_3_MASK_ADDR = 0;

	lclvec_init();

	setlclvector(VECTOR_ACFAIL, powerfail, 0);
	setlclvector(VECTOR_POWER, powerintr, 0);
	setlclvector(VECTOR_LCL2, lcl2_intr, 0);
#if 0			/* XXX - no one on lcl3 yet */
	setlclvector(VECTOR_LCL3, lcl3_intr, 0);
#endif
	/* Causing headaches for Indigo 2, enable from ISDN driver */
/*	setlclvector(VECTOR_HPCDMA, hpcdma_intr, 0); */

	/* set interrupt DMA burst and delay values */
	i_dma_burst = dma_delay;
	i_dma_delay = dma_burst;

	init_sysid();
	load_nvram_tab();	/* get a copy of the nvram */

	cachewrback = 1;
	/*
	 * cachecolormask is set from the bits in a vaddr/paddr
	 * which the r4k looks at to determine if a VCE should
	 * be raised.
	 * Despite what some documents may say, these are
	 * bits 12..14
	 * These are shifted down to give a value between 0..7
	 */
	cachecolormask = R4K_MAXPCACHESIZE/NBPP - 1;
#ifdef R4000PC
	if (private.p_scachesize == 0)
		cachecolormask = (pdcache_size / NBPP) - 1;
#endif
#ifdef R4600
	if (two_set_pcaches)
		cachecolormask = (two_set_pcaches / NBPP) - 1;
#endif
#ifdef _VCE_AVOIDANCE
	if (private.p_scachesize == 0)
		vce_avoidance = 1;
#ifdef R4600SC
	if (two_set_pcaches)
		vce_avoidance = 1;
#endif
#endif
#ifdef R4600
	/* 
	 * On the R4600, be sure the wait sequence is on a single cache line 
	 */
	if (two_set_pcaches) {
		extern int wait_for_interrupt_fix_loc[];
		int	i;

		if ((((__psint_t)&wait_for_interrupt_fix_loc[0]) & 0x1f) >= 0x18) {
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

	/* If not stuned, do not automatically reboot on panic */
	if (reboot_on_panic == -1)
		reboot_on_panic = 0;

	set_endianness();
	softpowerup_ok = 1;		/* for syssgi() */
	VdmaInit();

	if (is_fullhouse())
		eisa_init();

	memconfig_0 = *(volatile uint *)PHYS_TO_K1(MEMCFG0);
	memconfig_1 = *(volatile uint *)PHYS_TO_K1(MEMCFG1);
}


static void
set_endianness(void)
{
    int little_endian = 0;
    volatile unsigned int testaddr;
    char *cp = (char *)&testaddr;

    testaddr = 0x11223344;
    if (*cp == 0x44)
	little_endian = 1;

    if (little_endian)
	*(volatile char *)PHYS_TO_K1(HPC3_MISC_ADDR) |= HPC3_DES_ENDIAN;
    else
	*(volatile char *)PHYS_TO_K1(HPC3_MISC_ADDR) &= ~HPC3_DES_ENDIAN;
}

/*
 * setkgvector - set interrupt handler for the profiling clock
 */
void
setkgvector(void (*func)())
{
	c0vec_tbl[6].isr = func;
}

/*
 * setlclvector - set interrupt handler for given local interrupt level
 * Note that the graphics code only calls this with 2 args, but the
 * graphics interrupt routine that is passed doesn't expect an arg,
 * so that doesn't matter.
 */
void
setlclvector(int level, lcl_intr_func_t *func, __psint_t arg)
{
	int s;
	register lclvec_t *lv_p;	/* Which table entry to update */
	volatile unchar *mask_reg;	/* Addr of mask to enable int */
	int lcl_id;			/* Which lcl intr */

	/* Depending on the level, we need to set up a mask, and put
	 * the function into a particular jump table.  Each mask register
	 * and jump table has 8 entries.
	 */
	lcl_id = level/8;
	level &= 7;

	if (lcl_id == 0) {
		lv_p = &lcl0vec_tbl[level+1];
		mask_reg = K1_LIO_0_MASK_ADDR;
	} else if (lcl_id == 1) {
		lv_p = &lcl1vec_tbl[level+1];
		mask_reg = K1_LIO_1_MASK_ADDR;
	} else if (lcl_id == 2) {
		lv_p = &lcl2vec_tbl[level+1];
		mask_reg = K1_LIO_2_MASK_ADDR;
	} else {
		ASSERT(0);	/* No lcl3 interrupts yet. */
	}

	if (func) {
	    if (lv_p->lcl_flags & THD_ISTHREAD) {
		    atomicSetInt(&lv_p->lcl_flags, THD_REG);
		    if (ithreads_enabled)
			    lclvec_thread_setup(level, lcl_id, lv_p);
	    }
	    s = splhintr();
	    lv_p->isr = func;
	    lv_p->arg = arg;
	    *mask_reg |= 1 << level;
	} else {
/* If we're going to eliminate the thread handling this interrupt, this
 * is the place to do it.
 */
	    s = splhintr();
	    lv_p->isr = lcl_stray;
	    lv_p->arg = lcl_id*8 + level + 1;
	    *mask_reg &= ~(1 << level);
	}
		    
	flushbus();
	splx(s);
}

static struct {
	int vect;
	lcl_intr_func_t *ip22_func;
} giointr[] = {
	VECTOR_GIO0, ip22_gio0_intr,		/* GIO_INTERRUPT_0 */
	VECTOR_GIO1, ip22_gio1_intr,		/* GIO_INTERRUPT_1 */
	VECTOR_GIO2, ip22_gio2_intr		/* GIO_INTERRUPT_2 */
};

#if GIO_INTERRUPT_2 != 2
#error giointr[] depends on GIO_INTERRUPT_X layout!
#endif

void
setgiovector(int level, int slot, void (*func)(__psint_t, struct eframe_s *),
	     __psint_t arg)
{
	int s;

	if ( level < 0 || level >= GIOLEVELS )
		cmn_err(CE_PANIC,"GIO vector number out of range (%u)",level);
	
	if ( slot < 0 || slot >= GIOSLOTS )
		cmn_err(CE_PANIC,"GIO slot number out of range (%u)",slot);

	s = splgio2();

	if (is_fullhouse()) {
		/* IP22: There are two [or three] identical gio slots that
		 *	 all share interrupts at INT2/3.  A seperate register
		 *	 is read at interrupt time to determine which slot[s]
		 *	 have interrupts pending, so we need to call a fan
		 *	 out function first.
		 */
		giovec_tbl[slot][level].isr = func;
		giovec_tbl[slot][level].arg = arg;

		if (giovec_tbl[GIO_SLOT_GFX][level].isr ||
		    giovec_tbl[GIO_SLOT_0][level].isr ||
		    giovec_tbl[GIO_SLOT_1][level].isr) {
			setlclvector(giointr[level].vect,
				     giointr[level].ip22_func,
				     PHYS_TO_K1(HPC3_EXT_IO_ADDR));
		}
		else {
			setlclvector(giointr[level].vect,0,0);
		}
	}
	else {
		/* IP24: GFX slot is special with 3 dedicated gio interrupts,
		 *	 and each expansion slot has a single dedicated
		 *	 interrupt so each gio interrupt maps directly to
		 *	 a physical interrupt (no fan out needed).
		 */
		switch(slot) {
		case GIO_SLOT_GFX:
			setlclvector(giointr[level].vect,func,arg);
			break;
		case GIO_SLOT_0:
			setlclvector(VECTOR_GIOEXP0,func,arg);
			break;
		case GIO_SLOT_1:
			setlclvector(VECTOR_GIOEXP1,func,arg);
			break;
		}
	}

	splx(s);
}

/*
 * setgioconfig - Set GIO slot configuration register
 *
 */
void
setgioconfig(int slot, int flags)
{
	u_int mask;

	mask = (GIO64_ARB_EXP0_SIZE_64 | GIO64_ARB_EXP0_RT |
		GIO64_ARB_EXP0_MST);

	if (!is_fullhouse())
		mask |= GIO64_ARB_EXP0_PIPED;

	switch (slot) {
	case GIO_SLOT_0:
		break;
	case GIO_SLOT_1:
		mask <<= 1;
		flags <<= 1;
		break;
	case GIO_SLOT_GFX:
		mask >>= 1;
		flags >>= 1;
		break;
	default:
		ASSERT(slot == GIO_SLOT_0 || slot == GIO_SLOT_1 || slot == GIO_SLOT_GFX);
		return;
	}

        gio64_arb_reg = (gio64_arb_reg & ~mask) | (flags & mask);
	writemcreg(GIO64_ARB, gio64_arb_reg);
}

/*
 * Register a HPC dmadone interrupt handler for the given channel.
 */
void
sethpcdmaintr(int channel, lcl_intr_func_t *func, __psint_t arg)
{
    int s;
    int i;
    lcl_intr_func_t *hpcdma_handler;
    
    if (channel < 0 || channel >= HPCDMAVECTBL_SLOTS) {
	 cmn_err(CE_WARN, "sethpcdmaintr: bad hpcdmadone channel %d, ignored",
		 channel);
	 return;
    }

    s = splhintr();
    if (func) {
	 hpcdmavec_tbl[channel+1].isr = func;
	 hpcdmavec_tbl[channel+1].arg = arg;
    } else {
	 hpcdmavec_tbl[channel+1].isr = hpcdmastray;
	 hpcdmavec_tbl[channel+1].arg = 0;
    }
    hpcdma_handler = NULL;
    for (i = 0; i < HPCDMAVECTBL_SLOTS; i++)
    {
	lcl_intr_func_t *isr = hpcdmavec_tbl[channel+1].isr;

	if ((__psunsigned_t)isr != (__psunsigned_t)hpcdmastray && 
	    (__psunsigned_t)isr != (__psunsigned_t)hpcdmastray_scsi) {   
		hpcdma_handler = hpcdma_intr;
	    	break;
	}
    }
    setlclvector(VECTOR_HPCDMA, hpcdma_handler, 0);
    flushbus();
    splx(s);
}

/*
 * find first interrupt
 *
 * This is an inline version of the ffintr routine.  It differs in
 * that it takes a single 8 bit value instead of a shifted value.
 */
static char ffintrtbl[][16] = { { 0,5,6,6,7,7,7,7,8,8,8,8,8,8,8,8 },
			        { 0,1,2,2,3,3,3,3,4,4,4,4,4,4,4,4 } };
#define FFINTR(c) (ffintrtbl[0][c>>4]?ffintrtbl[0][c>>4]:ffintrtbl[1][c&0xf])

#ifdef DEBUG
/* count which interrupts we are getting, not just total.  Subtract
 * one because handler array starts at offset 1. */
unsigned intrcnt[8], intrl0cnt[8], intrl1cnt[8];
#define COUNT_INTR	intrcnt[(iv-c0vec_tbl)-1]++
#define COUNT_L0_INTR	intrl0cnt[(liv-lcl0vec_tbl)-1]++
#define COUNT_L1_INTR	intrl1cnt[(liv-lcl1vec_tbl)-1]++
#else
#define COUNT_INTR
#define COUNT_L0_INTR
#define COUNT_L1_INTR
#endif /* DEBUG */

/*
 * intr - handle 1st level interrupts
 */
/*ARGSUSED2*/
int
intr(struct eframe_s *ep, uint code, uint sr, uint cause)
{
	int ioc = is_ioc1();
	int check_kp = 0;
	int s;

	LOG_TSTAMP_EVENT(RTMON_INTR,TSTAMP_EV_INTRENTRY,NULL,NULL,NULL,NULL);

	/* I think this is needed for the R4000 count/compare WAR */
	if (ioc) {
		(void)get_r4k_counter();
	}

	while (1) {
		register struct intvec_s *iv;

		if (ioc &&
		    ((cause_ip5_count && (sr & CAUSE_IP5)) ||
		     (cause_ip6_count && (sr & CAUSE_IP6)))) {
			s = spl7();
			if (! (cause & CAUSE_IP5) &&
			    cause_ip5_count &&
			    (sr & CAUSE_IP5)) {
				cause_ip5_count--;
				cause |= CAUSE_IP5;
			}
			if (! (cause & CAUSE_IP6) &&
			    cause_ip6_count &&
			    (sr & CAUSE_IP6)) {
				cause_ip6_count--;
				cause |= CAUSE_IP6;
			}
			if (! (cause & (CAUSE_IP5 | CAUSE_IP6)))
				splx(s);
		}

		cause = (cause & sr) & SR_IMASK;

		if (!cause)
			break;

		iv = c0vec_tbl+FFINTR((cause >> CAUSE_IPSHIFT));
		COUNT_INTR;
		splx(iv->msk);
		(*iv->isr)(ep);

		if (iv->msk == (SR_IMASK5|SR_IEC)) {
			check_kp = 1;
		}
		atomicAddInt((int *) &SYSINFO.intr_svcd,1);
		cause &= ~(iv->bit << CAUSE_IPSHIFT);

		cause |= getcause();
	}

	/*
	 * try to trigger a later interrupt, if there are pending
	 * pseudo-interrupts we cannot deliver (due to their being
	 * masked), in case they are masked due to a raised spl
	 * at process level.
	 *
	 * NOTE:  This is also done in the ANON_ITHREAD code.
	 */
	if (ioc && (cause_ip5_count || cause_ip6_count))
		siron(CAUSE_SW2);

	LOG_TSTAMP_EVENT(RTMON_INTR,TSTAMP_EV_INTREXIT,TSTAMP_EV_INTRENTRY,
			 NULL,NULL,NULL);

#ifdef SPLMETER
	if (check_kp)
		_cancelsplhi();
#endif

	return check_kp;
}


#ifdef ITHREAD_LATENCY
#define SET_ISTAMP(liv) xthread_set_istamp(liv->lcl_lat)
#else
#define SET_ISTAMP(liv)
#endif /* ITHREAD_LATENCY */

#define LCL_CALL_ISR(liv,ep,maskaddr,threaded)			\
	if (liv->lcl_flags & THD_OK) {				\
		SET_ISTAMP(liv);				\
		*(maskaddr) &= ~(liv->bit);			\
		vsema(&liv->lcl_isync);				\
	} else {						\
		(*liv->isr)(liv->arg,ep);			\
	}

/*
 * lcl?intr - handle local interrupts 0 and 1
 */
static void
lcl0_intr(struct eframe_s *ep)
{
	/* Avoid some is_ioc3() calls */
	volatile unchar *int2mask = K1_LIO_0_MASK_ADDR;
	register lclvec_t *liv;
	char lcl_cause;
	int svcd = 0;

	lcl_cause = *K1_LIO_0_ISR_ADDR & *int2mask;

	if (lcl_cause) {
		do {
			liv = lcl0vec_tbl+FFINTR(lcl_cause);
			COUNT_L0_INTR;
			LCL_CALL_ISR(liv,ep,int2mask,threaded);
			svcd++;
			lcl_cause &= ~liv->bit;
		} while (lcl_cause);
	} else {
		/*
		 * INT2 fails to latch fifo full in the local interrupt
		 * status register, but it does latch it internally and
		 * needs to be cleared by masking it.  So if it looks like
		 * we've received a phantom local 0 interrupt, handle it
		 * as a fifo full.
		 */
		liv = &lcl0vec_tbl[VECTOR_GIO0+1];

		LCL_CALL_ISR(liv,ep,int2mask,threaded);

		return;
	}

	/*
	 * Check if we have a spurious/missed SCSI interrupt?
	 *
	 * The INDY product suffers from SCSI reset errors on the motherboard
	 * WD93 chip.  Testing in the factory found that at least some of
	 * these errors are caused by a missed SCSI interrupt from LCL0.
	 *
	 * We used to call wd93intr() to see if the intr was pending.
	 * Now, since we're already joined at the hips with the hardware,
	 * let's just check it directly.  Calling wd93intr() to check the bit
	 * is wasteful, and, in the threads world, is downright evil.
	 */
#define WD_0_INT_PRESENT	(*scuzzy[0].d_addr & AUX_INT)
	if (is_fullhouse() == 0) {
		if (WD_0_INT_PRESENT) {
			liv = &lcl0vec_tbl[VECTOR_SCSI+1];
				LCL_CALL_ISR(liv,ep,int2mask,threaded);
		}
	}

	/* account for extra one counted in intr */
	atomicAddInt((int *) &SYSINFO.intr_svcd, svcd-1);
}

static void
lcl1_intr(struct eframe_s *ep)
{
	/* Avoid some is_ioc3() calls */
	volatile unchar *int2mask = K1_LIO_1_MASK_ADDR;
	char lcl_cause;
	int svcd = 0;

	lcl_cause = *K1_LIO_1_ISR_ADDR & *int2mask;

	while (lcl_cause) {
		register lclvec_t *liv = lcl1vec_tbl+FFINTR(lcl_cause);
		COUNT_L1_INTR;
		LCL_CALL_ISR(liv,ep,int2mask,threaded);
		svcd++;
		lcl_cause &= ~liv->bit;
	}

	/* account for extra one counted in intr */
	atomicAddInt((int *) &SYSINFO.intr_svcd, svcd-1);
}

/*ARGSUSED1*/
static void
lcl2_intr(__psint_t arg, struct eframe_s *ep)
{
	/* Avoid some is_ioc3() calls */
	volatile unchar *int2mask = K1_LIO_2_MASK_ADDR;
	char lc2_cause;
	int svcd = 0;

	lc2_cause = *K1_LIO_2_ISR_ADDR & *int2mask;

	while (lc2_cause) {
		register lclvec_t *liv = lcl2vec_tbl+FFINTR(lc2_cause);
		LCL_CALL_ISR(liv,ep,int2mask,threaded);
		svcd++;
		lc2_cause &= ~liv->bit;
	}

	/* one counted in lcl[01]_intr */
	atomicAddInt((int *) &SYSINFO.intr_svcd, svcd-1);
}

#define HPC3_INTSTAT_ADDR2 0x1fbb000c

/*ARGSUSED1*/
void
hpcdma_intr(__psint_t arg, struct eframe_s *ep)
{
     uint dmadone, dmadone2, selector=1;
     int offset=1;
     register lclvec_t *liv;

     /* 
      * Bug in HPC3 requires me to read two registers and 'or' the halves
      * together.
      */
     dmadone = * (uint *) PHYS_TO_K1(HPC3_INTSTAT_ADDR);
     dmadone2 = * (uint *) PHYS_TO_K1(HPC3_INTSTAT_ADDR2);
     dmadone = (dmadone2 & 0x3e0) | (dmadone & 0x1f);

     if (dmadone) {
	  do {
	       if (dmadone & selector) {
		    liv = &hpcdmavec_tbl[offset];
		    (*liv->isr)(liv->arg,ep);
		    dmadone &= ~selector;
	       }
	       selector = selector << 1;
	       offset++;
	  } while (dmadone);
     }
}

static void
ip22_gio0_intr(__psint_t arg_extio, struct eframe_s *ep)
{
	uint extio = *(volatile uint *)arg_extio;

	/* mask the interrupt off */
	*((volatile unchar *)PHYS_TO_K1(LIO_0_MASK_ADDR)) &= ~LIO_FIFO;

	if ( ((extio & EXTIO_SG_IRQ_2) == 0) &&
	     giovec_tbl[GIO_SLOT_GFX][GIO_INTERRUPT_0].isr ) {
		((lcl_intr_func_t*)giovec_tbl[GIO_SLOT_GFX][GIO_INTERRUPT_0].isr)
		    (giovec_tbl[GIO_SLOT_GFX][GIO_INTERRUPT_0].arg,ep);
	}

	if ( ((extio & EXTIO_S0_IRQ_2) == 0) &&
	     giovec_tbl[GIO_SLOT_0][GIO_INTERRUPT_0].isr ) {
		((lcl_intr_func_t*)giovec_tbl[GIO_SLOT_0][GIO_INTERRUPT_0].isr)
		    (giovec_tbl[GIO_SLOT_0][GIO_INTERRUPT_0].arg,ep);
	}

	/* check isr first since original IP22 does not set EXTIO_S1_IRQ_2 */
	if (giovec_tbl[GIO_SLOT_1][GIO_INTERRUPT_0].isr &&
	     ((extio & EXTIO_S1_IRQ_2) == 0)) {
		((lcl_intr_func_t*)giovec_tbl[GIO_SLOT_1][GIO_INTERRUPT_0].isr)
		    (giovec_tbl[GIO_SLOT_1][GIO_INTERRUPT_0].arg,ep);
	}

	/* re-enable interrupt */
	*((volatile unchar *)PHYS_TO_K1(LIO_0_MASK_ADDR)) |= LIO_FIFO;
}
static void
ip22_gio1_intr(__psint_t arg_extio, struct eframe_s *ep)
{
	uint extio = *(volatile uint *)arg_extio;

	if ( ((extio & EXTIO_SG_IRQ_1) == 0) &&
	     giovec_tbl[GIO_SLOT_GFX][GIO_INTERRUPT_1].isr ) {
		(*giovec_tbl[GIO_SLOT_GFX][GIO_INTERRUPT_1].isr)
		    (giovec_tbl[GIO_SLOT_GFX][GIO_INTERRUPT_1].arg,ep);
	}

	if ( ((extio & EXTIO_S0_IRQ_1) == 0) &&
	     giovec_tbl[GIO_SLOT_0][GIO_INTERRUPT_1].isr ) {
		(*giovec_tbl[GIO_SLOT_0][GIO_INTERRUPT_1].isr)
		    (giovec_tbl[GIO_SLOT_0][GIO_INTERRUPT_1].arg,ep);
	}

	/* check isr first since original IP22 does not set EXTIO_S1_IRQ_1 */
	if ( giovec_tbl[GIO_SLOT_1][GIO_INTERRUPT_1].isr &&
	     ((extio & EXTIO_S1_IRQ_1) == 0) ) {
		(*giovec_tbl[GIO_SLOT_1][GIO_INTERRUPT_1].isr)
		    (giovec_tbl[GIO_SLOT_1][GIO_INTERRUPT_1].arg,ep);
	}
}

static void
ip22_gio2_intr(__psint_t arg_extio, struct eframe_s *ep)
{
	uint extio = *(volatile uint *)arg_extio;

	if ( ((extio & EXTIO_SG_RETRACE) == 0) &&
	     giovec_tbl[GIO_SLOT_GFX][GIO_INTERRUPT_2].isr ) {
		(*giovec_tbl[GIO_SLOT_GFX][GIO_INTERRUPT_2].isr)
		    (giovec_tbl[GIO_SLOT_GFX][GIO_INTERRUPT_2].arg,ep);
	}

	if ( ((extio & EXTIO_S0_RETRACE) == 0) &&
	     giovec_tbl[GIO_SLOT_0][GIO_INTERRUPT_2].isr ) {
		(*giovec_tbl[GIO_SLOT_0][GIO_INTERRUPT_2].isr)
		    (giovec_tbl[GIO_SLOT_0][GIO_INTERRUPT_2].arg,ep);
	}

	/* check isr first since original IP22 does not set EXTIO_S1_RETRACE */
	if ( giovec_tbl[GIO_SLOT_1][GIO_INTERRUPT_2].isr &&
	     ((extio & EXTIO_S1_RETRACE) == 0) ) {
		(*giovec_tbl[GIO_SLOT_1][GIO_INTERRUPT_2].isr)
		    (giovec_tbl[GIO_SLOT_1][GIO_INTERRUPT_2].arg,ep);
	}

}

static int printstray0;

/*ARGSUSED*/
static void
lcl_stray(__psint_t lvl, struct eframe_s *ep)
{
	static int stray0ints;
	int lcl_id = (lvl-1)/8;
	lvl = (lvl-1)&7;

	if (lcl_id == 0) {
		if(printstray0)	/* so we can turn on with symmon or dbx */
			cmn_err(CE_WARN, "stray local 0 interrupt #%d\n", lvl);
		stray0ints++;	/* for crash dumps, etc. */
	} else {
		cmn_err(CE_WARN, "stray local %d interrupt #%d\n",lcl_id, lvl);
	}
}

/*ARGSUSED*/
static void
hpcdmastray(__psint_t adap, struct eframe_s *ep)
{
	cmn_err(CE_WARN,"stray HPC dmadone interrupt on pbus chan %d\n",adap);
}

/*ARGSUSED*/
static void
hpcdmastray_scsi(__psint_t adap, struct eframe_s *ep)
{
     uint clear;

     if (adap == 9) {
	  clear = * (uint *) PHYS_TO_K1(HPC3_SCSI_CONTROL0);	  
     } else {
	  clear = * (uint *) PHYS_TO_K1(HPC3_SCSI_CONTROL1);
     }
     clear &= 0xff;

     if (clear & SCPARITY_ERR) {
	  cmn_err(CE_WARN, "stray parity error on scsi chan %d, control bits 0x%x", adap - 9, clear);
     } else {
	  cmn_err(CE_WARN, "stray HPC dmadone interrupt on scsi chan %d, control bits 0x%x", adap - 9, clear);
     }
}

/*ARGSUSED*/
static void
powerfail(__psint_t arg, struct eframe_s *ep)
{
	int adap;
	extern int wd93cnt;
	int i;

	/*  Some power supplies will give spurious short lived (~250ns)
	 * ACFAIL interrupts.  Check twice to see if the interrupt is
	 * still present.  If not, we probably have a flakey supply,
	 * so disable the ACFAIL interrupt.  We need to try twice since
	 * if we get unlucky we can check when there is more noise
	 * on the signal.  Spikes are usually ~9.5us apart, and the
	 * loop here takes ~4us, so we should land between spikes.
	 */
	for (i=0; i < 2; i++) {
		DELAY(2);
		if (!((*K1_LIO_1_ISR_ADDR)&LIO_AC)) {
			if (showconfig)
				cmn_err(CE_WARN,"spurious power failure "
					"interrupt -- Check power supply.\n");
			setlclvector(VECTOR_ACFAIL,0,0);
			return;
		}
	}

	/* maybe should spin a bit before complaining?  Sometimes
	 * see this when people power off.  May want to use this
	 * for 'clean' shutdowns if we ever get enough power to
	 * write out part of the superblocks, or whatever.  For
	 * now, reset SCSI bus to attempt to ensure we don't wind
	 * up with a media error due to a block header write being
	 * in progress when the power actually fails. */
	for(adap=0; adap<wd93cnt; adap++)
		wd93_resetbus(adap);
	cmn_err_tag(33,CE_WARN,"Power failure detected\n");
}


/* function to shut off flat panel, set in newport_init.c */
int (*fp_poweroff)(void);

/* can be called via a timeout from powerintr(), direct from prom_reboot
 * in arcs.c, or from mdboot() as a result of a uadmin call.
*/
void
cpu_powerdown(void)
{
	volatile uint *p = (uint *)PHYS_TO_K1(HPC3_PANEL);
	register ds1286_clk_t *clock = (ds1286_clk_t *)RTDS_CLOCK_ADDR;
	volatile int dummy;
	
	/* shut off the flat panel, if one attached */
	if (fp_poweroff)
	  (*fp_poweroff)();

	/*  Some power supplies give ACFAIL when soft powered down */
	setlclvector(VECTOR_ACFAIL,0,0);

	/* disable watchdog output so machine knows it was turned off */
	clock->command |= WTR_DEAC_WAM;
	clock->watch_hundreth_sec = 0;
	clock->watch_sec = 0;

	splhi();

	while(1) {
		*p = ~POWER_SUP_INHIBIT;	/* sets POWER_INT */
		flushbus();
		DELAY(10);

		/* If power not out, maybe "wakeupat" time hit the window.
		 */
		if (clock->command & WTR_TDF) {
			/* Disable and clear alarm */
			clock->command |= WTR_DEAC_TDM;
			flushbus();
			dummy = clock->hour_alarm;
		}
	}
}


static void
cpu_godown()
{
	cmn_err(CE_WARN,
		"init 0 did not shut down after %d seconds, powering off.\n",
		clean_shutdown_time);
	DELAY(500000);	/* give them 1/2 second to see it */
	cpu_powerdown();
}

/* While looping, check if the power button is being pressed. */
void
cpu_waitforreset(void)
{
	volatile unchar *i = (unchar *)PHYS_TO_K1(LIO_1_ISR_ADDR);
	volatile uint *p = (uint *)PHYS_TO_K1(HPC3_PANEL);

	while (1) {
	    if (*i & LIO_POWER)
		if (is_fullhouse() || ((*p & POWER_INT) == 0))
		    cpu_powerdown();
	}
}

#ifdef POWER_DUMP
void
power_dump(int checkintr)
{
	int s = splprof();
	volatile uint *p = (uint *)PHYS_TO_K1(HPC3_PANEL);

	if (checkintr) {
	    if ((*p & POWER_INT) && !(*K1_LIO_1_ISR_ADDR & LIO_POWER))
		return;
	}

	/* re-enable the interrupt */
	*p = POWER_INT|POWER_SUP_INHIBIT;
	flushbus();

	/* wait for switch to be released */
	while (*K1_LIO_1_ISR_ADDR & LIO_POWER) {
	    *p = POWER_ON;
	    flushbus();
	    DELAY(5);
	}
	cmn_err(CE_PANIC, "Power Button induced crash from %s\n",
		checkintr ? "r4kcounter_intr":"powerintr");
	splx(s);
}
#endif

/*ARGSUSED*/
void
powerintr(__psint_t arg, struct eframe_s *ep)
{
	volatile uint *p = (uint *)PHYS_TO_K1(HPC3_PANEL);
	static time_t doublepower;
	extern int power_off_flag;	/* set in powerintr(), or uadmin() */
	int fh;

	if ( (fh=is_fullhouse()) || ((*p & POWER_INT) == 0) ) {

#ifdef POWER_DUMP
		if (power_button_changed_to_crash_dump)
			power_dump(0);
#endif

		/* for prom_reboot() in ars.c */
		power_off_flag = 1;

		/* re-enable the interrupt */
		*p = POWER_INT|POWER_SUP_INHIBIT;
		flushbus();

		if (!doublepower) {
			if (fh) power_bell();		/* ack button press */
			flash_led(3);

			/* wait for switch to be released */
			while (*K1_LIO_1_ISR_ADDR & LIO_POWER) {
				*p = POWER_ON;
				flushbus();
				DELAY(5);
			}

			/*  Send SIGINT to init, which is the same as
			 * '/etc/init 0'.  Set a timeout
			 * in case init wedges.  Set doublepower to handle
			 * quick (with a little debounce room) 'doubleclick'
			 * on the power switch for an immediate shutdown.
			 */
			if (!sigtopid(INIT_PID, SIGINT, SIG_ISKERN, 0, 0, 0)) {
				timeout(cpu_godown, 0, clean_shutdown_time*HZ);
				doublepower = lbolt + HZ;
				set_autopoweron();
			}
			else {
				DELAY(10000);		/* 10ms debounce time */
				cpu_powerdown();
			}
		}
		else if (lbolt > doublepower) {
			if (fh) power_bell();

			/* wait for switch to be released */
			while (*K1_LIO_1_ISR_ADDR & LIO_POWER) {
				*p = POWER_ON;
				flushbus();
				DELAY(5);
			}

			DELAY(10000);		/* 10ms debounce time */

			cmn_err(CE_NOTE, "Power switch pressed twice -- "
					 "shutting off power now.\n");

			cpu_powerdown();
		}
		return;
	}

	if (!(*p & PANEL_VOLUME_UP_INT) || !(*p & PANEL_VOLUME_DOWN_INT)) {

		/* re-enable the  volume control buttons' interrupt */
		*p |= PANEL_VOLUME_UP_INT | PANEL_VOLUME_DOWN_INT |
		POWER_SUP_INHIBIT;

		if (volumectrl_ptr) {

			/* mask off the panel interrupt which includes
			   power button, volume up and down buttons  */
			 *K1_LIO_1_MASK_ADDR &= ~LIO_POWER;

			/* query the current volume level from the
		   	   driver, the valid flag number is  > 3 */
			curlevel = (* volumectrl_ptr)(0x04, 0);
			volumeintr(&curlevel);
		}
	}
}

/*  Flash LED from green to off to indicate system powerdown is in
 * progress.
 */
static void
flash_led(int on)
{
	set_leds(on);
	timeout(flash_led,(void *)(on ? 0 : (__psint_t)3),HZ/4);
}

/*
 * log_perr - log the bank, byte, and SIMM of a detected parity error
 *	addr is the contents of the cpu or dma parity error address register
 *	bytes is the contents of the parity error register
 *
 */
#define	BYTE_PARITY_MASK	0xff
static char bad_simm_string[16];

void
log_perr(uint addr, uint bytes, int no_console, int print_help)
{
    int bank = addr_to_bank(addr);
    int fh = is_fullhouse();
    int print_prio = print_help ? CE_ALERT : CE_CONT;
    
    static char *simm_fh[3][4] = {
	" S12", " S11", " S10", " S9",
	" S8",  " S7",  " S6",  " S5",
	" S4",  " S3",  " S2",  " S1",
    };
    static char *simm_g[2][4] = {
	" S1", " S2", " S3", " S4",
	" S5", " S6", " S7", " S8",
    };

    bad_simm_string[0] = '\0';

    if (bank < 0) {
	if (no_console)
	    cmn_err(CE_CONT, "!Parity Error in Unknown SIMM\n");
	else
	    cmn_err(CE_CONT, "Parity Error in Unknown SIMM\n");
	return;
    }

    if ((bytes & 0xf0) && !(addr & 0x8))
	strcat(bad_simm_string, fh? simm_fh[bank][0] : simm_g[bank][0]);
    if ((bytes & 0x0f) && !(addr & 0x8))
	strcat(bad_simm_string, fh? simm_fh[bank][1] : simm_g[bank][1]);
    if ((bytes & 0xf0) && (addr & 0x8))
	strcat(bad_simm_string, fh? simm_fh[bank][2] : simm_g[bank][2]);
    if ((bytes & 0x0f) && (addr & 0x8))
	strcat(bad_simm_string, fh? simm_fh[bank][3] : simm_g[bank][3]);

    if (no_console)
    	cmn_err(print_prio, "!Memory Parity Error in SIMM %s\n",
		bad_simm_string);
    else
    	cmn_err(print_prio, "Memory Parity Error in SIMM %s\n",
		bad_simm_string);

    return;
}

extern void fix_bad_parity(volatile uint *);
extern char *dma_par_msg;

#ifdef SPECIAL_BUS_ERROR_HANDLER
/*
 * We allow a customer's
 * driver to specify a bus-error handling function.  This function
 * is called only if the system is about to panic.  
 */
int (*special_bus_error_handler)(void*,void**) = NULL;
caddr_t special_bus_error_ef_epc = NULL;

int (*set_special_bus_error_handler(int (*new_handler)(void *,void **)))(void *,void **)
{
	int (*old_handler)(void*,void**);

	old_handler = special_bus_error_handler;
	special_bus_error_handler = new_handler;
	return old_handler;
}
#endif /* SPECIAL_BUS_ERROR_HANDLER */

/*
 * dobuserre - Common IP22 bus error handling code
 *
 * The MC sends an interrupt whenever bus or parity errors occur.  
 * In addition, if the error happened during a CPU read, it also
 * asserts the bus error pin on the R4K.  Code in VEC_ibe and VEC_dbe
 * save the MC bus error registers and then clear the interrupt
 * when this happens.  If the error happens on a write, then the
 * bus error interrupt handler saves the info and comes here.
#ifdef SPECIAL_BUS_ERROR_HANDLER
 * 
 * In SPECIAL_BUS_ERROR_HANDLER mode, if the bus error is handled,
 * this routine (and dobuserr_common) returns 2.
 *
#endif
 *
 * flag:	0: kernel; 1: kernel - no fault; 2: user
 */

#define GIO_ERRMASK	0xff00
#define CPU_ERRMASK	0x3f00

uint hpc3_ext_io, hpc3_bus_err_stat;
uint cpu_err_stat, gio_err_stat;
uint cpu_par_adr, gio_par_adr;
static uint bad_data_hi, bad_data_lo;

static int dobuserr_common(struct eframe_s *, inst_t *, uint, uint);

int
dobuserre(struct eframe_s *ep, inst_t *epc, uint flag)
{
	if (flag == 1)
		return(0); /* nofault */

	return(dobuserr_common(ep,epc,flag,/* is_exception = */ 1));
}

static int
dobuserr_common(struct eframe_s *ep,
		inst_t *epc,
		uint flag,
		uint is_exception)
{
	int cpu_buserr = 0;
	int cpu_memerr = 0;
	int gio_buserr = 0;
	int gio_memerr = 0;
	int fatal_error = 0;
	caddr_t mapped_bad_addr = NULL;
	int sysad_was_enabled;
#ifdef R4600SC
	int r4600sc_scache_disabled = 1;

	if (two_set_pcaches && private.p_scachesize)
		r4600sc_scache_disabled = _r4600sc_disable_scache();
#endif
	
#ifdef _MEM_PARITY_WAR
#ifdef DEBUG
	cmn_err(CE_CONT, "In dobuserr_common, err %x addr %x exception %x\n", 
		cpu_err_stat, cpu_par_adr, is_exception);
#endif
	/* Initiate recovery if it is a memory parity error.
	 */
	if ((cpu_err_stat & CPU_ERR_STAT_RD_PAR) == CPU_ERR_STAT_RD_PAR) {
	    if (!no_parity_workaround) {
	    	if (eccf_addr == NULL)
		    if (! perr_save_info(ep,NULL,CACHERR_EE,(k_machreg_t) epc,
				 (is_exception
				   ? (((ep->ef_cause & CAUSE_EXCMASK) ==
				    	EXC_IBE) 
				        ? PERC_IBE_EXCEPTION
				   	: PERC_DBE_EXCEPTION)
				   : PERC_BE_INTERRUPT)))
			cmn_err_tag(34,CE_PANIC,"Fatal parity error\n");
			
	    	ASSERT(eccf_addr);		/* perr_save_info left this */

	    	if (ecc_exception_recovery(ep)) {
#ifdef R4600SC
			if (!r4600sc_scache_disabled)
				_r4600sc_enable_scache();
#endif
		    	return 1;
	        }
		
	    } else if (dwong_patch(ep)) {
#ifdef R4600SC
			if (!r4600sc_scache_disabled)
				_r4600sc_enable_scache();
#endif
			return 1;	/* survived it */
	    }
	}
#endif /* _MEM_PARITY_WAR */

	/*
	 * we print all gruesome info for 
	 *   1. debugging kernel
	 *   2. bus errors
	 */
#ifdef _MEM_PARITY_WAR
	if (kdebug || ((cpu_err_stat & CPU_ERRMASK) && !ecc_perr_is_forced()))
#else
	if (kdebug || ((cpu_err_stat & CPU_ERRMASK)))
#endif
		cmn_err(CE_CONT, "CPU Error/Addr 0x%x<%s%s%s%s%s%s>: 0x%x\n",
			cpu_err_stat,
			cpu_err_stat & CPU_ERR_STAT_RD ? "RD " : "",
			cpu_err_stat & CPU_ERR_STAT_PAR ? "PAR " : "",
			cpu_err_stat & CPU_ERR_STAT_ADDR ? "ADDR " : "",
			cpu_err_stat & CPU_ERR_STAT_SYSAD_PAR ? "SYSAD " : "",
			cpu_err_stat & CPU_ERR_STAT_SYSCMD_PAR ? "SYSCMD " : "",
			cpu_err_stat & CPU_ERR_STAT_BAD_DATA ? "BAD_DATA " : "",
			cpu_par_adr);
	if (kdebug || (gio_err_stat & GIO_ERRMASK))
		cmn_err(CE_CONT, "GIO Error/Addr 0x%x:<%s%s%s%s%s%s%s%s> 0x%x\n",
			gio_err_stat,
			gio_err_stat & GIO_ERR_STAT_RD ? "RD " : "",
			gio_err_stat & GIO_ERR_STAT_WR ? "WR " : "",
			gio_err_stat & GIO_ERR_STAT_TIME ? "TIME " : "",
			gio_err_stat & GIO_ERR_STAT_PROM ? "PROM " : "",
			gio_err_stat & GIO_ERR_STAT_ADDR ? "ADDR " : "",
			gio_err_stat & GIO_ERR_STAT_BC ? "BC " : "",
			gio_err_stat & GIO_ERR_STAT_PIO_RD ? "PIO_RD " : "",
			gio_err_stat & GIO_ERR_STAT_PIO_WR ? "PIO_WR " : "",
			gio_par_adr);
	if (kdebug || (hpc3_ext_io & EXTIO_HPC3_BUSERR))
		cmn_err(CE_CONT, "HPC3 Bus Error 0x%x\n",hpc3_bus_err_stat);
	if (kdebug || (hpc3_ext_io & EXTIO_EISA_BUSERR))
		cmn_err(CE_CONT, "EISA Bus Error 0x%x\n",(hpc3_ext_io & EXTIO_EISA_BUSERR));	/* EIU info here? */

	/*
	 * We are here because the CPU did something bad - if it
	 * read a parity error, then the CPU bit will be on and
	 * the CPU_ERR_ADDR register will contain the faulting word
	 * address. If a parity error happened during an GIO transaction, then
	 * GIO_ERR_ADDR will contain the bad memory address.
	 */
	if (cpu_err_stat & CPU_ERR_STAT_ADDR)
		cpu_buserr = 1;
	else if ((cpu_err_stat & (CPU_ERR_STAT_RD | CPU_ERR_STAT_PAR))
	    == (CPU_ERR_STAT_RD | CPU_ERR_STAT_PAR))
		cpu_memerr = 1;
	else if (cpu_err_stat & CPU_ERRMASK)
		fatal_error = 1;

	if (gio_err_stat & (GIO_ERR_STAT_TIME | GIO_ERR_STAT_PROM))
		gio_buserr = 1;
	else if (gio_err_stat & (GIO_ERR_STAT_RD|GIO_ERR_STAT_PIO_RD))
		gio_memerr = 1;
	else if (gio_err_stat & GIO_ERRMASK)
		fatal_error = 1;

	if (hpc3_ext_io & (EXTIO_HPC3_BUSERR|EXTIO_EISA_BUSERR))
		fatal_error = 1;

	/* print bank, byte, and SIMM number if it's a parity error
	 */
#ifdef _MEM_PARITY_WAR
	if (cpu_memerr && !ecc_perr_is_forced()) {
#else
	if (cpu_memerr) {
#endif
		log_perr(cpu_par_adr, cpu_err_stat, 0, 1);

		mapped_bad_addr = map_physaddr((paddr_t) (cpu_par_adr & ~0x7));
#ifdef	_MEM_PARITY_WAR
		sysad_was_enabled = is_sysad_parity_enabled();
		disable_sysad_parity();
#endif	/* _MEM_PARITY_WAR */
		bad_data_hi = *(volatile uint *)mapped_bad_addr;
		bad_data_lo = *(volatile uint *)(mapped_bad_addr + 4);
		*(volatile uint *)PHYS_TO_K1(CPU_ERR_STAT) = 0;
		flushbus();

		/* print pretty message */
		if (flag == 2 && curvprocp) {
			cmn_err(CE_ALERT,
"Process %d [%s] sent SIGBUS due to Memory Error in SIMM %s\n\tat Physical Address 0x%x, Data: 0x%x/0x%x\n",
				current_pid(), get_current_name(),
				bad_simm_string, 
				cpu_par_adr, bad_data_hi, bad_data_lo);
			fix_bad_parity((volatile uint *) mapped_bad_addr);
		} else
			cmn_err_tag(35, CE_PANIC,
"IRIX Killed due to Memory Error in SIMM %s\n\tat Physical Address 0x%x PC:0x%x ep:0x%x\n\tMemory Configuration registers: 0x%x/0x%x, Data: 0x%x/0x%x\n",
				bad_simm_string, cpu_par_adr, epc, ep, 
				memconfig_0, memconfig_1,
				bad_data_hi, bad_data_lo);
#ifdef	_MEM_PARITY_WAR
		if (sysad_was_enabled)
			enable_sysad_parity();
#endif	/* _MEM_PARITY_WAR */
		unmap_physaddr(mapped_bad_addr);
	}

#ifdef _MEM_PARITY_WAR
	if (cpu_memerr && ecc_perr_is_forced()) {
		/* print pretty message */
		if (flag == 2 && curvprocp) {
			cmn_err(CE_ALERT,
"Process %d [%s] sent SIGBUS due to uncorrectable cache error\n",
				current_pid(), get_current_name());
		} else
			cmn_err(CE_PANIC,
"IRIX Killed due to uncorrectable cache error\n");
	}
#endif /* _MEM_PARITY_WAR */

	if (gio_memerr) {
		log_perr(gio_par_adr, gio_err_stat, 0, 1);

		mapped_bad_addr = map_physaddr((paddr_t) (gio_par_adr & ~0x7));
#ifdef	_MEM_PARITY_WAR
		sysad_was_enabled = is_sysad_parity_enabled();
		disable_sysad_parity();
#endif	/* _MEM_PARITY_WAR */
		bad_data_hi = *(volatile uint *)mapped_bad_addr;
		bad_data_lo = *(volatile uint *)(mapped_bad_addr + 4);
		*(volatile uint *)PHYS_TO_K1(CPU_ERR_STAT) = 0;
		flushbus();

		/* print pretty message */
		if (flag == 2 && curvprocp) {
			cmn_err(CE_CONT,
"Process %d [%s] sent SIGBUS due to Memory Error in SIMM %s\n\tat Physical Address 0x%x, Data: 0x%x/0x%x\n",
				current_pid(), get_current_name(), 
				bad_simm_string, 
				gio_par_adr, bad_data_hi, bad_data_lo);
			fix_bad_parity((volatile uint *) mapped_bad_addr);
		} else
			cmn_err(CE_PANIC,
"IRIX Killed due to Memory Error in SIMM %s\n\tat Physical Address 0x%x PC:0x%x ep:0x%x\n\tMemory Configuration registers: 0x%x/0x%x, Data: 0x%x/0x%x\n",
				bad_simm_string, gio_par_adr, epc, ep, 
				memconfig_0, memconfig_1,
				bad_data_hi, bad_data_lo);
#ifdef	_MEM_PARITY_WAR
		if (sysad_was_enabled)
			enable_sysad_parity();
#endif	/* _MEM_PARITY_WAR */
		unmap_physaddr(mapped_bad_addr);
	}

	if (cpu_buserr) {
		if (flag == 2 && curvprocp)
			cmn_err(CE_CONT,
"Process %d [%s] sent SIGBUS due to Bus Error\n",
				current_pid(), get_current_name());
#ifdef SPECIAL_BUS_ERROR_HANDLER
		/*
		 * If special_bus_error_handler is set (which happens
		 * only in the special customer driver), we give the handler 
		 * the opportunity to prevent panicing.
		 */
		else if ((gio_buserr) && (special_bus_error_handler != NULL) &&
		        (special_bus_error_handler(
			    mapped_bad_addr = map_physaddr(cpu_par_adr),
			    (void**)&special_bus_error_ef_epc) != 0)) {
		        cmn_err(CE_CONT, "CPU bus error handled\n");
			unmap_physaddr(mapped_bad_addr);
		    	return(2);
		}
#endif /* SPECIAL_BUS_ERROR_HANDLER */
		else
			cmn_err(CE_PANIC,
"IRIX Killed due to Bus Error\n\tat PC:0x%x ep:0x%x\n", 
				epc, ep);
	}

	if (gio_buserr) {
		if (flag == 2 && curvprocp)
			cmn_err(CE_CONT,
"Process %d [%s] sent SIGBUS due to Bus Error\n",
				current_pid(), get_current_name());
#ifdef SPECIAL_BUS_ERROR_HANDLER
		/*
		 * If special_bus_error_handler is set (which happens
		 * only in the special customer driver), we give the handler 
		 * the opportunity to prevent panicing.
		 */
		else if ((special_bus_error_handler != NULL) &&
		    	(special_bus_error_handler(
				mapped_bad_addr = map_physaddr(gio_par_adr),
				(void**)&special_bus_error_ef_epc) != 0)) {
			cmn_err(CE_CONT, "GIO bus error handled\n");
			unmap_physaddr(mapped_bad_addr);
			return(2);
		}
#endif /* SPECIAL_BUS_ERROR_HANDLER */
		else
			cmn_err(CE_PANIC,
"IRIX Killed due to Bus Error\n\tat PC:0x%x ep:0x%x\n", 
				epc, ep);
	}

	if (fatal_error)
#ifdef SPECIAL_BUS_ERROR_HANDLER
	    /*
	     * If special_bus_error_handler is set (which happens
	     * only in the special customer driver), we give the handler
	     * the opportunity to prevent panicing.
	     */
            if ((hpc3_ext_io & (EXTIO_EISA_BUSERR)) &&
                (special_bus_error_handler != NULL) &&
                (special_bus_error_handler(
		    mapped_bad_addr = NULL,
		    (void**)&special_bus_error_ef_epc) != 0))
                {
			cmn_err(CE_CONT, "EISA bus error handled\n");
			/* unmap_physaddr(mapped_bad_addr); */
                	return(2);
                }
            else
#endif /* SPECIAL_BUS_ERROR_HANDLER */
		cmn_err(CE_PANIC,
"IRIX Killed due to internal Error\n\tat PC:0x%x ep:0x%x\n", 
			epc, ep);

#ifdef R4600SC
	if (!r4600sc_scache_disabled)
		_r4600sc_enable_scache();
#endif

	return(0);
}

#ifndef _MEM_PARITY_WAR
#define	CPU_RD_PAR_ERR	(CPU_ERR_STAT_RD | CPU_ERR_STAT_PAR)
ulong kv_initial_from;
ulong kv_initial_to;
ulong initial_count;
#endif /* ! _MEM_PARITY_WAR */

/*
 * dobuserr - handle bus error interrupt
 *
 * flag:  0 = kernel; 1 = kernel - no fault; 2 = user
 *
 * Save error register information in globals.  Clear
 * MC error interrupt.
 */
void
err_save_regs(void)
{
	/* save parity info */
	cpu_err_stat = *(volatile uint *)PHYS_TO_K1(CPU_ERR_STAT);
	gio_err_stat = *(volatile uint *)PHYS_TO_K1(GIO_ERR_STAT);
	cpu_par_adr  = *(volatile uint *)PHYS_TO_K1(CPU_ERR_ADDR);
	gio_par_adr  = *(volatile uint *)PHYS_TO_K1(GIO_ERR_ADDR);

	hpc3_bus_err_stat = *(volatile uint *)PHYS_TO_K1(HPC3_BUSERR_STAT_ADDR);

	hpc3_ext_io = *(volatile uint *)PHYS_TO_K1
		(is_fullhouse() ? HPC3_EXT_IO_ADDR : HPC3_INT3_ERROR_STATUS);

	/*
	 * Clear the bus error by writing to the parity error register
	 * in case we're here due to parity problems.
	 */
	*(volatile uint *)PHYS_TO_K1(CPU_ERR_STAT) = 0;
	*(volatile uint *)PHYS_TO_K1(GIO_ERR_STAT) = 0;
	flushbus();
}

/*
 * dobuserr - handle bus error interrupt
 *
 * flag:  0 = kernel; 1 = kernel - no fault; 2 = user
 */
int
dobuserr(struct eframe_s *ep, inst_t *epc, uint flag)
{
	err_save_regs();

	return(dobuserr_common(ep, epc, flag, /* is_exception = */ 0));
}

/*
 * Should never receive an exception (other than interrupt) while
 * running on the idle stack.
 * Check for memory errors
 */
void
idle_err(inst_t *epc, uint cause, void *k1, void *sp)
{
	if ((cause & CAUSE_EXCMASK) == EXC_IBE ||
	    (cause & CAUSE_EXCMASK) == EXC_DBE)
		dobuserre(NULL, epc, 0);
	else if (cause & CAUSE_BERRINTR)
		dobuserr(NULL, epc, 0);

	cmn_err(CE_PANIC,
	"exception on IDLE stack k1:0x%x epc:0x%x cause:0x%w32x sp:0x%x badvaddr:0x%x",
		k1, epc, cause, sp, getbadvaddr());
	/* NOTREACHED */
}

/*
 * earlynofault - handle very early global faults
 * Returns: 1 if should do nofault
 *	    0 if not
 */
earlynofault(struct eframe_s *ep, uint code)
{
	switch(code) {
	case EXC_DBE:
		dobuserre(ep, (inst_t *)ep->ef_epc, 1);
		break;
	default:
		return(0);
	}
	return(1);
}

/*
 * fp_find - probe for floating point chip
 */
void
fp_find(void)
{
	private.p_fputype_word = get_fpc_irr();
}

/*
 * fp chip initialization
 */
void
fp_init(void)
{
	set_fpc_csr(0);
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

/*  npage and flags aren't used for IP22, since it is only used for SCSI
 * and EISA.
 */
/*ARGSUSED4*/
dmamap_t *
dma_mapalloc(int type, int adap, int npage, int flags)
{
	static dmamap_t smap[NSCUZZY];
	dmamap_t *dmamap;

	switch (type) {

	    case DMA_SCSI:
		if (adap >= NSCUZZY)
			return((dmamap_t *)-1L);
		smap[adap].dma_type = DMA_SCSI;
		smap[adap].dma_adap = adap;
		return &smap[adap];

	    case DMA_EISA:
		if (adap != 0)
			return((dmamap_t *)-1L);
		dmamap = (dmamap_t *) kern_calloc(1, sizeof(dmamap_t));

		dmamap->dma_type = DMA_EISA;
		dmamap->dma_adap = 0;
		dmamap->dma_size = npage;
		return (dmamap);

	    default:
		return((dmamap_t *)-1L);
	}
}

/*  Macros to support multiple maximum HPC SCSI DMA sizes.  For large
 * pages (16K+) we can map the page in 4K or 8K DMA descriptors.
 *
 * NOTE: Can't use HPC3 8K pages on IP22 for HPC1 compatability.  Added
 *	 to prototype large DMAs for teton.
 */
#if NBPP == 4096			/* 4K pages, do max 4K transfers */
#define HPC_NBPP	NBPC
#define HPC_BPCSHIFT	BPCSHIFT
#define HPC_NBPC	NBPC
#define HPC_POFFMASK	POFFMASK
#define HPC_PGFCTR	PGFCTR
#define hpc_poff(x)	poff(x)
#define hpc_ctob(x)	ctob(x)
#define hpc_btoc(x)	btoc(x)
#define hpc_btoct(x)	btoct(x)
#elif HPC8KSCSIDMA			/* large pages, use max DMA size */
#define _USE_HPCSUBPAGE	1
#define HPC_NBPP	8192
#define HPC_BPCSHIFT	13
#define HPC_NBPC	HPC_NBPP
#define HPC_POFFMASK	(HPC_NBPP - 1)
#define HPC_PGFCTR	(NBPP / HPC_NBPP)
#define hpc_poff(x)	((__psunsigned_t)(x) & HPC_POFFMASK)
#define hpc_ctob(x)	((x)<<HPC_BPCSHIFT)
#define hpc_btoc(x)	(((__psunsigned_t)(x)+(HPC_NBPC-1))>>HPC_BPCSHIFT)
#define hpc_btoct(x)	((__psunsigned_t)(x)>>HPC_BPCSHIFT)
#else					/* large pages, use common DMA size */
#define _USE_HPCSUBPAGE	1
#define HPC_NBPP	IO_NBPP
#define HPC_BPCSHIFT	IO_BPCSHIFT
#define HPC_NBPC	IO_NBPC
#define HPC_POFFMASK	IO_POFFMASK
#define HPC_PGFCTR	PGFCTR
#define hpc_poff(x)	io_poff(x)
#define hpc_ctob(x)	io_ctob(x)
#define hpc_btoc(x)	io_btoc(x)
#define hpc_btoct(x)	io_btoct(x)
#endif

#if _USE_HPCSUBPAGE
/* Macro for extracting HPC 'sub-page' on large page sized systems */
#define HPC_PAGENUM(A)		(((__psunsigned_t)(A)&POFFMASK)&~HPC_POFFMASK)
#define HPC_PAGEMAX		HPC_PAGENUM(0xffffffff)
#endif

/*
 *	Map `len' bytes from starting address `addr' for a SCSI DMA op.
 *	Returns the actual number of bytes mapped.
 *
 *	NOTE: Can't use HPC3 8K pages on IP22 for HPC1 compatability.
 */
int
dma_map(dmamap_t *map, caddr_t addr, int len)
{
	scuzzy_t *scp = &scuzzy[map->dma_adap];
	uint npages;

	switch (map->dma_type) {
	    case DMA_SCSI: {
		unsigned int incr, bytes, dotrans, npages, partial;
		unsigned int *cbp, *bcnt;
		pde_t *pde;

		/* can only deal with word aligned addresses */
		if ((__psunsigned_t)addr & 0x3)
			return(0);

		/* setup descriptor pointers */
		cbp = (uint *)(map->dma_virtaddr + scp->d_cbpoff);
		bcnt = (uint *)(map->dma_virtaddr + scp->d_cntoff);

		/* for wd93_go */
		map->dma_index = 0;

		if(bytes = hpc_poff(addr)) {
			if((bytes = (HPC_NBPC - bytes)) > len) {
				bytes = len;
				npages = 1;
			}
			else
				npages = hpc_btoc(len-bytes) + 1;
		}
		else {
			bytes = 0;
			npages = hpc_btoc(len);
		}
		if(npages > (map->dma_size-1)) {
			/* -1 for HPC3_EOX_VALUE */
			npages = map->dma_size-1;
			partial = 0;
			len = (npages-(bytes>0))*HPC_NBPC + bytes;
		}
		else if(bytes == len) {
			/* only one descr, and that is part of a page */
			partial = bytes;	
		} else {
			/* partial page in last descr? */
			partial = (len - bytes) % HPC_NBPC;	
		}

		/* We either have KSEG2 or KSEG0/1/Phys, figure out now */
		if (IS_KSEG2(addr)) {
			pde = kvtokptbl(addr);
			dotrans = 1;
		} else {
			/* We either have KSEG0/1 or Physical */
			if (IS_KSEGDM(addr)) {
				addr = (caddr_t)KDM_TO_PHYS(addr);
			}
			dotrans = 0;
		}

		/* Generate SCSI dma descriptors for this request */
		if (bytes) {
#if !_USE_HPCSUBPAGE
			*cbp = dotrans ? pdetophys(pde++) + poff(addr) : 
				(__psunsigned_t)addr;
#else
			if (dotrans) {
				/* Use poff to keep io sub-page */
				*cbp = pdetophys(pde) + poff(addr);
				if (HPC_PAGENUM(addr) == HPC_PAGEMAX) pde++;
			}
			else
				*cbp = (__psunsigned_t)addr;
#endif
			addr += bytes;
			*bcnt = bytes;
			cbp += sizeof (scdescr_t) / sizeof (uint);
			bcnt += sizeof (scdescr_t) / sizeof (uint);
			npages--;
		}
		while (npages--) {
#if !_USE_HPCSUBPAGE
			*cbp = dotrans ? pdetophys(pde++) 
				: (__psunsigned_t)addr;
#else
			if (dotrans) {
				__psint_t io_pn = HPC_PAGENUM(addr);
				*cbp = pdetophys(pde) + io_pn;
				if (io_pn == HPC_PAGEMAX) pde++;
			}
			else
				*cbp = (__psunsigned_t)addr;
#endif
			incr = (!npages && partial) ? partial : HPC_NBPC;
			addr += incr;
			*bcnt = incr;
			cbp += sizeof (scdescr_t) / sizeof (uint);
			bcnt += sizeof (scdescr_t) / sizeof (uint);
		}
	
		/* set EOX flag and zero BCNT on hpc3 for 25mhz workaround */
		if (scp->d_reset == SCSI_RESET) {
			*bcnt = HPC3_EOX_VALUE;
		} else {
			/* go back and set EOX on last desc for HPC1 */
			cbp -= sizeof (scdescr_t) / sizeof (uint);
			*cbp |= HPC1_EOX_VALUE;
		}

		/* flush descriptors back to memory so HPC gets right data */
		__dcache_wb_inval((void *)map->dma_virtaddr,
			(dmavaddr_t)(bcnt+2) - map->dma_virtaddr);

		return len;
	}

	case DMA_EISA:
		map->dma_addr = (__psunsigned_t)addr;
		npages = numpages(addr, len);

		if (IS_KSEGDM(addr)) {
			if (npages > map->dma_size)
				len = map->dma_size*NBPC - poff(addr);
		} else {
			if (npages > 1)
				len = NBPC - poff(addr);
		}
		return (len);

	default:
		return 0;
	}
}

/* avoid overhead of testing for KDM each time through; this is
	the second part of kvtophys() */
#define KNOTDM_TO_PHYS(X) \
	((pnum((X) - K2SEG) < (__psunsigned_t)syssegsz) ? \
		(__psunsigned_t)pdetophys((pde_t*)kvtokptbl(X))+poff(X) : \
		(__psunsigned_t)(X))

dmamaddr_t
dma_mapaddr(dmamap_t *map, caddr_t addr)
{
	if (map->dma_type != DMA_EISA)
		return 0;

	if (IS_KSEGDM(addr))
		return (KDM_TO_PHYS(addr));
	else
		return (KNOTDM_TO_PHYS(addr));
}

void
dma_mapfree(dmamap_t *map)
{
	if (map && map->dma_type == DMA_EISA)
		kern_free(map);
}

/*
 * dma_mapbp -	Map `len' bytes of a buffer for a SCSI DMA op.
 *		Returns the actual number of bytes mapped.
 * Only called when a SCSI request is started, unlike the other
 * machines, except when the request is larger than we can fit
 * in a single dma map.  In those cases, the offset will always be
 * page aligned, same as at the start of a request.
 * While starting addresses will always be page aligned, the last
 * page may be a partial page.
 *
 *	NOTE: Can't use HPC3 8K pages on IP22 for HPC1 compatability.
 */
dma_mapbp(dmamap_t *map, buf_t *bp, int len)
{
	scuzzy_t *scp = &scuzzy[map->dma_adap];
	register struct pfdat *pfd = NULL;
	register uint bytes, npages;
	register unsigned *cbp, *bcnt;
	register int		xoff;	/* bytes already xfered */
#if _USE_HPCSUBPAGE
	int io_pfd_cnt = HPC_PGFCTR;	/* count of HPC_NBPP sub-pages */
#endif

	/* for wd93_go */
	map->dma_index = 0;

	/* setup descriptor pointers */
	cbp = (uint *)(map->dma_virtaddr + scp->d_cbpoff);
	bcnt = (uint *)(map->dma_virtaddr + scp->d_cntoff);

	/* compute page offset into xfer (i.e. pages to skip this time) */
	xoff = bp->b_bcount - len;
	npages = hpc_btoct(xoff);
	while (npages--) {
#if !_USE_HPCSUBPAGE
		pfd = getnextpg(bp, pfd);
		{
#else
		if (++io_pfd_cnt >= HPC_PGFCTR) {
			pfd = getnextpg(bp, pfd);
			io_pfd_cnt = 0;
#endif
			if (pfd == NULL) {
				cmn_err(CE_WARN,
					"dma_mapbp: !pfd, bp 0x%x len 0x%x",
					bp, len);
				return -1;
			}
		}
	}
		
	xoff = poff(xoff);
	npages = hpc_btoc(len + xoff);
	if (npages > (map->dma_size-1)) {
		npages = map->dma_size-1;
		len = hpc_ctob(npages);
	}
	bytes = len;

	while (npages--) {
#if !_USE_HPCSUBPAGE
		pfd = getnextpg(bp, pfd);
		{
#else
		if (++io_pfd_cnt >= HPC_PGFCTR) {
			pfd = getnextpg(bp, pfd);
			io_pfd_cnt = 0;
#endif
			if (!pfd) {
				/*  This has been observed to happen on
				 * occasion when we somehow get a 45/{48,49}
				 * interrupt, but the count hadn't dropped
				 * to 0, and therefore we try to go past the
				 * end of the page list, hitting invalid pages.
				 * Since this seems to be happening just a bit
				 * more often than I like, and it is very
				 * difficult to reproduce, for now we issue a
				 * warning and fail the request rather than
				 * panic'ing.
				 */
				cmn_err(CE_WARN, "dma_mapbp: Invalid mapping: "
						 "pfd==0, bp %x len %x",
					bp, len);
				return -1;
			}
		}
#if !_USE_HPCSUBPAGE
		*cbp = pfdattophys(pfd) + xoff;
#else
		*cbp = pfdattophys(pfd) | (io_pfd_cnt*HPC_NBPP);
#endif
		*bcnt = MIN(bytes, HPC_NBPC - xoff);
		cbp += sizeof (scdescr_t) / sizeof (uint);
		bcnt += sizeof (scdescr_t) / sizeof (uint);
		bytes -= HPC_NBPC - xoff;
		xoff = 0;
	}

	/* set EOX flag and zero BCNT on hpc3 for 25mhz workaround */
	if (scp->d_reset == SCSI_RESET) {
		*bcnt = HPC3_EOX_VALUE;
	} else {
		/* go back and set EOX on last desc for HPC1 */
		cbp -= sizeof (scdescr_t) / sizeof (uint);
		*cbp |= HPC1_EOX_VALUE;
	}

	/*
	 * Need a final descriptor with byte cnt of zero and EOX flag set
	 * for the HPC3 to work right.  Should be harmless on others...
	 */

	/* flush descriptors back to memory so HPC gets right data */
	__dcache_wb_inval((void *)map->dma_virtaddr,
		(dmavaddr_t)(bcnt + 2) - map->dma_virtaddr);

	return len;
}

int wd93hpc1highmemok = 0;

/*
 * This is an init routine for the GIO SCSI expansion board for INDY.
 */
static void
wd93_hpc1init(int adap)
{
	scuzzy_t *scp = &scuzzy[adap];
	__psunsigned_t hpc_addr;
	uint nohpc, val;
	volatile unchar *ap, *dp;

	/* get the HPC base address */
	hpc_addr = (__psunsigned_t)scp->d_ctrl & ~0xffff;

	/* the INT2 port on the GIO scsi card is wired to 0x1 */
	nohpc = badaddr_val((void *)(hpc_addr + HPC1_LIO_0_OFFSET),
		sizeof (int), (int *)&val);
	if (nohpc || val != 0x1) {
		scp->d_vector = -1;
		return;
	}

	/* check if high memory exists (>256MB) and driver loaded */
	if ((physmem > 0x10000) && !wd93hpc1highmemok) {
		scp->d_vector = -1;
		return;
	}

	/* set HPC-1 endianess */
#ifdef _MIPSEB
	*(volatile uint *)(hpc_addr + HPC1_ENDIAN_OFFSET) = 0x0;
#else
	*(volatile uint *)(hpc_addr + HPC1_ENDIAN_OFFSET) = 0x1f;
#endif	/* _MIPSEB */

	/*
	 * unless you really know what you're doing,
	 * do not mess with the next ~20 lines of
	 * code. they are duplicate of the SCSI init
	 * code found in the PROM csu.s
	 */

	/* reset the SCSI bus */
        *scp->d_ctrl = scp->d_reset;
	flushbus();
	DELAY(25);
        *scp->d_ctrl = 0;
	flushbus();

	/* reset the WD93 chip */
	ap = scp->d_addr;
	dp = scp->d_data;
	*ap = WD93STAT;		/* clear any interrupt by reading */
	*dp;			/* the status reg, need ~7 usecs */
	DELAY(7);		/* delay between status read and cmd */
	*dp = C93RESET;
	flushbus();
	DELAY(25);

	/*
	 * Do basic disk driver setup now
	 */
	if (wd93_earlyinit(adap))
		return;

	/*
	 * Configure slot now that we know it is a GIO SCSI board
	 */
	setgioconfig(adap - GIOSCSI, GIO64_ARB_EXP0_RT | GIO64_ARB_EXP0_MST);
	setgiovector(scp->d_vector, adap-GIOSCSI, (lcl_intr_func_t *)wd93intr, adap);
}	


int    wd95cnt;
struct giogfx_s {
	void	(*isr)(int, struct eframe_s *);
	int	  arg;
} giogfx_tbl[5] ={{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}};

/*ARGSUSED1*/
void
giogfx_intr(__psint_t unused, struct eframe_s *ep)
{
	register char	status;
	register char	mask = 0x8;
	register int	count;

	status = *((u_char *) PHYS_TO_K1(HPC31_INTRCFG));
	for (count=0; mask; count++, mask <<= 1) {
		if ((status & mask) && giogfx_tbl[count].isr)
			(*giogfx_tbl[count].isr)(giogfx_tbl[count].arg, ep);
	}

}

void
setgiogfxvec(const uint number, lcl_intr_func_t *isr, int arg)
{
	/* This routine is only called on challenge S machines with
	 * no graphics.  And, the routines must be threaded, since
	 * they are SCSI and ethernet which can call sleeplocks
	 */
	atomicSetInt(&lcl0vec_tbl[VECTOR_GIO0+1].lcl_flags, THD_ISTHREAD);
	
	if (number >= 5)
		return;
	giogfx_tbl[number].isr = isr;
	giogfx_tbl[number].arg = arg;
	setgiovector(0, GIO_SLOT_GFX, giogfx_intr, 0);
}

/*	This is a hack to make the Western Digital chip stop
 *	interrupting before we turn interrupts on.
 *	ALL IP22's have the 93B chip, and the fifo with the
 *	burstmode dma, so just set the variable.
 */
void
wd93_init(void)
{
	int		 adap;
	scuzzy_t	*scp;
	extern int	 wd93_earlyinit(int);
	extern int	 wd95_earlyinit(int);
	extern int	 wd93burstdma, wd93cnt;

	ASSERT(NSCUZZY <= SCSI_MAXCTLR);	/* sanity check */
	wd93burstdma = 1;

	/* three on guiness (plus hole), two on fullhouse. */
	wd93cnt = is_fullhouse() ? 2 : 4;

	/* early init, need low level before high
	 * level devices do their io_init's (needs some
	 * work for multiple SCSI on setlclvector) */
	for (adap=0; adap<wd93cnt; adap++) {
		/* init GIO expansion channels */
		if (adap >= GIOSCSI && adap <= GIOSCSIMAX) {
			wd93_hpc1init(adap);
			continue;
		}

		/* init mother board channels (must start with adap 0) */
		if (wd93_earlyinit(adap) == 0) {
			scp = &scuzzy[adap];
			setlclvector(scp->d_vector, 
				(lcl_intr_func_t *)wd93intr,
				adap);
		}
	}

	/*
	 * Determine if Challenge S dual wd95 controller present.  It sits
	 * at the same base address as the HPC1.5-based SCSI and Ethernet
	 * boards, so we check an address that the HPC3 likes but the HPC1.5
	 * does not like.
	 */
        if (badaddr((char *) HPC3_NOT_HPC1, sizeof(long)) == 0) {
		wd95cnt = 6;

		/*
		 * Set up to talk to second HPC3.
		 * If a GIO-32 SCSI or Enet board is in expansion slot 1, steal
		 *   DMA from slot 0.  Otherwise steal from slot one.
		 * Program gio64_arb register on MC chip.
		 * Then set up pbus pio cfg register so that we can set
		 *   up the config register on Challenge S I/O board.
		 */
		if (badaddr((char *) HPC_HOLLY_2_LIO, sizeof(int)) == 0) {
			gio64_arb_reg |= GIO64_ARB_EXP0_MST | GIO64_ARB_EXP0_SIZE_64 |
					 GIO64_ARB_HPC_EXP_SIZE_64;
			adap = 0x20;
		}
		else {
			gio64_arb_reg |= GIO64_ARB_EXP1_MST | GIO64_ARB_EXP1_SIZE_64 |
				         GIO64_ARB_HPC_EXP_SIZE_64;
			adap = 0x30;
		}
		writemcreg(GIO64_ARB, gio64_arb_reg);
		*((uint *) PHYS_TO_K1(HPC31_PBUS_PIO_CFG_0)) = 0x3ffff;
		*((uint *) PHYS_TO_K1(HPC31_INTRCFG)) = adap;

		*scuzzy[4].d_piocfg = WD95_PIO_CFG_33MHZ;
		*scuzzy[5].d_piocfg = WD95_PIO_CFG_33MHZ;
		*scuzzy[4].d_dmacfg = WD95_DMA_CFG_33MHZ;
		*scuzzy[5].d_dmacfg = WD95_DMA_CFG_33MHZ;
		*scuzzy[4].d_ctrl = 0;
		*scuzzy[5].d_ctrl = 0;
		if(wd95_earlyinit(4) == 0)
			setgiogfxvec(3, (void (*)())wd95intr, 4);
		if(wd95_earlyinit(5) == 0)
			setgiogfxvec(4, (void (*)())wd95intr, 5);

		/* Reset ethernet */
		*((uint *) PHYS_TO_K1(HPC31_ETHER_MISC_ADDR)) = 0x1;
	}
}	

/*
 * Reset the SCSI bus. Interrupt comes back after this one.
 * this also is tied to the reset pin of the SCSI chip.
 */
void
wd93_resetbus(int adap)
{
	scuzzy_t *sc = &scuzzy[adap];

	/* Ignore GIO-SCSI option cards that are not installed */
	if (sc->d_vector == (unsigned char)-1)
		return;

	*sc->d_ctrl = sc->d_reset;	/* write in the reset code */
	DELAY(25);			/* hold 25 usec to meet SCSI spec */
	*sc->d_ctrl = 0;		/* no dma, no flush */
}


/*
 * Set the DMA direction, and tell the HPC to set up for DMA.
 * Have to be sure flush bit no longer set.
 * dma_index will be zero after dma_map() is called, but may be
 * non-zero after dma_flush, when a target has re-connected partway
 * through a transfer.  This saves us from having to re-create the map
 * each time there is a re-connect.
 * addr arg isn't needed for IP22.
 */
/*ARGSUSED2*/
void
wd93_go(dmamap_t *map, caddr_t addr, int readflag)
{
	scuzzy_t *sc = &scuzzy[map->dma_adap];

	*sc->d_nextbp = (ulong)((scdescr_t*)map->dma_addr + map->dma_index);
        *sc->d_ctrl = readflag ? sc->d_rstart : sc->d_wstart;
}


/*
 * Allocate the per-device DMA descriptor list.
 * cmap is the dmamap returned from dma_mapalloc.
 */
dmamap_t *
scsi_getchanmap(dmamap_t *cmap, int npages)
{
	register scdescr_t *scp, *pscp;
	register dmamap_t *map;
	register int i;

	if (!cmap)
		return cmap;	/* paranoia */
	if (npages == 0)
		npages = maxdmasz;

	/*
	 * An individual descriptor can't cross a page boundary.
	 * This code is predicated on 16-byte descriptors and 128 byte
	 * cachelines.  We should never have a page descriptor that
	 * crosses a page boundary.
	 */
	map = (dmamap_t *) kern_malloc(sizeof(*map));
	i = (1 + npages) * sizeof(scdescr_t);
	map->dma_virtaddr = (caddr_t) kmem_alloc(i, VM_CACHEALIGN | VM_PHYSCONTIG);

	/* so we don't have to calculate each time in dma_map */
	map->dma_addr = kvtophys((void *)map->dma_virtaddr);
	map->dma_adap = cmap->dma_adap;
	map->dma_type = DMA_SCSI;
	map->dma_size = npages;

	/* fill in the next-buffer values now, we never change them */
	scp = (scdescr_t *)map->dma_virtaddr;
	pscp = (scdescr_t *)map->dma_addr + 1;
	for (i = 0; i < npages; ++i, ++scp, ++pscp) {
		scp->nbp = (__psunsigned_t)pscp;
	}

	return map;
}


dmamap_t *
wd93_getchanmap(register dmamap_t *cmap)
{
	return scsi_getchanmap(cmap, NSCSI_DMA_PGS);
}


/*
 * Flush the fifo to memory after a DMA read.
 * OK to leave flush set till next DMA starts.  SCSI_STARTDMA bit gets
 * cleared when fifo has been flushed.
 * We 'advance' the dma map chain when we are called, so that when
 * some data was transferred, but more remains we don't have
 * to reprogram the entire descriptor chain.
 * When reading, this is only valid after the flush has completed.  So
 * we check the SCSI_STARTDMA bit to be sure flush has completed.
 *
 * Note that when writing, the curbp can be considerably off,
 * since the WD chip could have asked for up to 15 extra bytes,
 * plus the HPC could have asked for up to 64 extra bytes.
 * Thus we might not even be on the 'correct' descriptor any
 * longer when writing.  We must therefore advance the map index
 * by calculation.  This is still faster than rebuilding the map.
 * Unlike standalone, keep the 'faster' method of figuring out where
 * we are when reading.
 *
 * Also check for a parity error after a DMA operation.
 * return 1 if there was one
*/
int
scsidma_flush(dmamap_t *map, int readflag)
{
	scuzzy_t *scp = &scuzzy[map->dma_adap];
	unsigned fcnt;

	if (readflag) {
		fcnt = 512;	/* normally only 2 or 3 times */
		*scp->d_ctrl |= scp->d_flush;
		while((*scp->d_ctrl & scp->d_busy) && --fcnt)
			;
		if(fcnt == 0)
			cmn_err(CE_PANIC,
			   "SCSI DMA in progress bit never cleared\n");
	}
	else
		/* turn off dma bit; mainly for cases where drive 
		 * disconnects without transferring data, and HPC already
		 * grabbed the first 64 bytes into it's fifo; otherwise gets
		 * the first 64 bytes twice, since it 'notices' that curbp was
		 * reset, but not that is the same.
		 */
		*scp->d_ctrl = 0;
	return 0;
}

int
wd93dma_flush(dmamap_t *map, int readflag, int xferd)
{
	scuzzy_t *scp = &scuzzy[map->dma_adap];
	unsigned *bcnt, *cbp, index, fcnt;

	if(readflag && xferd) {
		fcnt = 512;	/* normally only 2 or 3 times */
		*scp->d_ctrl |= scp->d_flush;
		while((*scp->d_ctrl & scp->d_busy) && --fcnt)
			;
		if(fcnt == 0)
			cmn_err(CE_PANIC,
			   "SCSI DMA in progress bit never cleared\n");
	}
	else
		/* turn off dma bit; mainly for cases where drive 
		* disconnects without transferring data, and HPC already
		* grabbed the first 64 bytes into it's fifo; otherwise gets
		* the first 64 bytes twice, since it 'notices' that curbp was
		* reset, but not that is the same.  */
		*scp->d_ctrl = 0;

	/* disconnect with no i/o */
	if (!xferd)
		return 0;

	/* index from last wd93_go */
	index = map->dma_index;
	fcnt = index * sizeof (scdescr_t);
	cbp = (uint *)(map->dma_virtaddr + scp->d_cbpoff + fcnt);
	bcnt = (uint *)(map->dma_virtaddr + scp->d_cntoff + fcnt);

	/* handle possible partial page in first descriptor */
	fcnt = *bcnt & (HPC_NBPP | (HPC_NBPP - 1));
	if (xferd >= fcnt) {
		xferd -= fcnt;
		cbp += sizeof (scdescr_t) / sizeof (uint);
		bcnt += sizeof (scdescr_t) / sizeof (uint);
		++index;
	}
	if (xferd) {
		/* skip whole descriptors */
		fcnt = xferd / HPC_NBPP;
		index += fcnt;
		fcnt *= sizeof (scdescr_t) / sizeof (uint);
		cbp += fcnt;
		bcnt += fcnt;

		/* create first partial page descriptor */
		*bcnt -= hpc_poff(xferd);
		*cbp += hpc_poff(xferd);

		/* flush descriptor back to memory so HPC gets right data */
		__dcache_wb_inval((void *)
			((scdescr_t *)map->dma_virtaddr + index),
			sizeof (scdescr_t));
	}

	/* prepare for rest of dma (if any) after next reconnect */
	map->dma_index = index;

#ifdef	LATER
	if(paritychk && !readflag &&
		(data=(*(uint *)PHYS_TO_K1(PAR_ERR_ADDR) & PAR_DMA))) {
		/* note that the parity error could have been from some other
		 * devices DMA also (parallel, or DSP) */
		readflag = *(volatile int *)PHYS_TO_K1(DMA_PAR_ADDR);
		log_perr(readflag, data, 0, 1);
		cmn_err(CE_WARN, dma_par_msg, readflag<<22);
		*(volatile u_char *)PHYS_TO_K1(PAR_CL_ADDR) = 0;	/* clear it */
		flushbus();
		return(1);
	}
	else
#endif	/* LATER */
		return(0);
}


/*
 * wd93_int_present - is a wd93 interrupt present
 * In machine specific code because of IP4
 */
wd93_int_present(wd93dev_t *dp)
{
	/* reads the aux register */
	return(*dp->d_addr & AUX_INT);
}


/*
 * Routines for LED handling
 */

/*
 * Initialize led counter and value
 */
void
reset_leds(void)
{
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

/*
 * Set the leds to a particular pattern.  Used during startup, and during
 * a crash to indicate the cpu state.  The SGI_SETLED can be used to make
 * it change color.  We don't allow UNIX to turn the LED off.  Also, UNIX
 * is not allowed to turn the LED *RED* (a big no no in Europe).
 */

static const char led_masks[4][2] = {
	/* Fullhouse	Guinness */
	{ LED_GREEN,	LED_RED_OFF },			/* green */
	{ LED_AMBER,	0 },				/* amber */
	{ LED_AMBER,	LED_GREEN_OFF },		/* amber/red */
	{ 0,		LED_RED_OFF|LED_GREEN_OFF }	/* off */
};

void
set_leds(int pattern)
{
	_hpc3_write1 &= ~(LED_AMBER|LED_GREEN);
	_hpc3_write1 |= led_masks[pattern?1:0][!is_fullhouse()];
	*(volatile uint *)PHYS_TO_K1(HPC3_WRITE1) = _hpc3_write1;
}

void bump_leds(void) {}

/*
 * sendintr - send an interrupt to another CPU; fatal error on IP22.
 */
/*ARGSUSED*/
int
sendintr(cpuid_t destid, unchar status)
{
	panic("sendintr");
	/*NOTREACHED*/
}


/* START of Time Of Day clock chip stuff (to "END of Time Of Day clock chip" */

static void _clock_func(int func, char *ptr);
static void setbadclock(void), clearbadclock(void);
static int isbadclock(void);
extern u_char miniroot;

/*
 * Get the time/date from the ds1286, and reinitialize
 * with a guess from the file system if necessary.
 */
void
inittodr(time_t base)
{
	register uint todr;
	long deltat;
	int TODinvalid = 0;
	extern int todcinitted;
	int s,dow;
	char buf[7];

	s = mutex_spinlock_spl(&timelock, splprof);

	todr = rtodc(); /* returns seconds from 1970 00:00:00 GMT */
	settime(todr,0);

	if (miniroot) {	/* no date checking if running from miniroot */
		mutex_spinunlock(&timelock, s);
		return;
	}

	/*
	 * If day of week not set then set it.
 	 */
	dow = get_dayof_week(todr);
	_clock_func(WTR_READ, buf);
	if (dow != buf[6]) {
		buf[6] = dow;
		_clock_func( WTR_WRITE, buf );
	}

	/*
	 * Complain if the TOD clock is obviously out of wack. It
	 * is "obvious" that it is screwed up if the clock is behind
	 * the 1988 calendar currently on my wall.  Hence, TODRZERO
	 * is an arbitrary date in the past.
	 */
	if (todr < TODRZERO) {
		if (todr < base) {
			cmn_err_tag(36, CE_WARN, "lost battery backup clock--using file system time");
			TODinvalid = 1;
		} else {
			cmn_err_tag(37, CE_WARN, "lost battery backup clock");
			setbadclock();
		}
	}

	/*
	 * Complain if the file system time is ahead of the time
	 * of day clock, and reset the time.
	 */
	if (todr < base) {
		if (!TODinvalid)	/* don't complain twice */
			cmn_err_tag(38, CE_WARN, "time of day clock behind file system time--resetting time");
		settime(base, 0);
		resettodr();
		setbadclock();
	}
	todcinitted = 1; 	/* for chktimedrift() in clock.c */

	/*
	 * See if we gained/lost two or more days: 
	 * If so, assume something is amiss.
	 */
	deltat = todr - base;
	if (deltat < 0)
		deltat = -deltat;
	if (deltat > 2*SECDAY)
		cmn_err_tag(39,CE_WARN,"clock %s %d days",
		    time < base ? "lost" : "gained", deltat/SECDAY);

	if (isbadclock())
		cmn_err_tag(40, CE_WARN, "CHECK AND RESET THE DATE!");

	mutex_spinunlock(&timelock, s);
}


/* 
 * Reset the TODR based on the time value; used when the TODR
 * has a preposterous value and also when the time is reset
 * by the stime system call.
 */

void
resettodr(void) {
	wtodc();
}

static int month_days[12] = {
	31,	/* January */
	28,	/* February */
	31,	/* March */
	30,	/* April */
	31,	/* May */
	30,	/* June */
	31,	/* July */
	31,	/* August */
	30,	/* September */
	31,	/* October */
	30,	/* November */
	31	/* December */
};

static void _clock_func(int func, char *ptr);
static int dallas_yrref = DALLAS_YRREF;

int
rtodc()
{
	uint month, day, year, hours, mins, secs;
	register int i;
	char buf[7];

	_clock_func(WTR_READ, buf);

	secs = (uint)( buf[ 0 ] & 0xf );
	secs += (uint)( buf[ 0 ] >> 4 ) * 10;

	mins = (uint)( buf[ 1 ] & 0xf );
	mins += (uint)( buf[ 1 ] >> 4 ) * 10;

	/*
	 * we use 24 hr mode, so bits 4 and 5 represent the 2 tens-hour bits
	 */
	hours = (uint)( buf[ 2 ] & 0xf );
	hours += (uint)( ( buf[ 2 ] >> 4 ) & 0x3 ) * 10;

	/*
	 * actually day of month
	 */
	day = (uint)( buf[ 3 ] & 0xf );
	day += (uint)( buf[ 3 ] >> 4 ) * 10;

	month = (uint)( buf[ 4 ] & 0xf );
	month += (uint)( buf[ 4 ] >> 4 ) * 10;

	year = (uint)( buf[ 5 ] & 0xf );
	year += (uint)( buf[ 5 ] >> 4 ) * 10;
	/*  Up till now, we stored the year as years since 1970.  This confuses
	 * dallas clock wrt leap year.  It will think 1994 is a leap year,
	 * instead of 1996.  If we appear to still be using YRREF as a base,
	 * update it to DALLAS_YRREF.
	 *
	 *  Do not update the time base in the miniroot.
	 */
	if (year < 45) {		/* 5.2 -> 13.7 in 2016 will break */
		year += (YRREF - DALLAS_YRREF);
		if (!miniroot) {
			buf[5] = (char)( ((year/10 )<<4) | (year%10) );
			_clock_func(WTR_WRITE, buf);
		}
		else
			dallas_yrref = YRREF;	/* keep timebase in miniroot */
	}
	year += DALLAS_YRREF;

	/*
	 * Sum up seconds from 00:00:00 GMT 1970
	 */
	secs += mins * SECMIN;
	secs += hours * SECHOUR;
	secs += (day-1) * SECDAY;

	if (LEAPYEAR(year))
		month_days[1]++;
	for (i = 0; i < month-1; i++)
		 secs += month_days[i] * SECDAY;
	if (LEAPYEAR(year))
		month_days[1]--;

	while (--year >= YRREF) {
		secs += SECYR;
		if (LEAPYEAR(year))
			secs += SECDAY;
	}
	return(secs);
}

void
wtodc(void)
{
	register long year_secs = time;
	register month, day, hours, mins, secs, year, dow;
	char buf[7];

        /*
         * calculate the day of the week
         */
        dow = get_dayof_week(year_secs);

	/*
	 * Whittle the time down to an offset in the current year,
	 * by subtracting off whole years as long as possible.
	 */
	year = YRREF;
	for (;;) {
		register secyr = SECYR;
		if (LEAPYEAR(year))
			secyr += SECDAY;
		if (year_secs < secyr)
			break;
		year_secs -= secyr;
		year++;
	}
	/*
	 * Break seconds in year into month, day, hours, minutes, seconds
	 */
	if (LEAPYEAR(year))
		month_days[1]++;
	for (month = 0;
	   year_secs >= month_days[month]*SECDAY;
	   year_secs -= month_days[month++]*SECDAY)
		continue;
	month++;
	if (LEAPYEAR(year))
		month_days[1]--;

	for (day = 1; year_secs >= SECDAY; day++, year_secs -= SECDAY)
		continue;

	for (hours = 0; year_secs >= SECHOUR; hours++, year_secs -= SECHOUR)
		continue;

	for (mins = 0; year_secs >= SECMIN; mins++, year_secs -= SECMIN)
		continue;

	secs = year_secs;

	buf[ 0 ] = (char)( ( ( secs / 10 ) << 4 ) | ( secs % 10 ) );

	buf[ 1 ] = (char)( ( ( mins / 10 ) << 4 ) | ( mins % 10 ) );

	buf[ 2 ] = (char)( ( ( hours / 10 ) << 4 ) | ( hours % 10 ) );

	buf[ 3 ] = (char)( ( ( day / 10 ) << 4 ) | ( day % 10 ) );

	buf[ 4 ] = (char)( ( ( month / 10 ) << 4 ) | ( month % 10 ) );

	/*
	 * The number of years since DALLAS_YRREF (1940) is what actually
	 * gets stored in the clock.  Leap years work on the ds1286 because
	 * the hardware handles the leap year correctly.
	 */
	year -= dallas_yrref;
	buf[ 5 ] = (char)( ( ( year / 10 ) << 4 ) | ( year % 10 ) );
	/*
	 * Now that we are done with year stuff, plug in day_of_week
	 */
	buf[ 6 ] = (char) dow;

	_clock_func( WTR_WRITE, buf );

	/*
	 * clear the flag in nvram that says whether or not the clock
	 * date is correct or not.
	 */
	clearbadclock();
}

static void
_clock_func(int func, char *ptr)
{
	register ds1286_clk_t *clock = RTDS_CLOCK_ADDR;
	int s = splhi();

	if (func == WTR_READ) {
		clock->command &= ~WTR_TE;		/* freeze external */

		/* read data */
		*ptr++ = (char)clock->sec;
		*ptr++ = (char)clock->min;
		*ptr++ = (char)clock->hour & 0x3f;
		*ptr++ = (char)clock->date & 0x3f;
		*ptr++ = (char)clock->month & 0x1f;
		*ptr++ = (char)clock->year;
		*ptr++ = (char)clock->day & 0x7;

	} else {
		/* make sure clock oscilltor is going. */
		clock->month &= ~WTR_EOSC_N;

		clock->command &= ~WTR_TE;		/* freeze external */

		/* write data */
		clock->sec = *ptr++;
		clock->min = *ptr++;
		clock->hour = *ptr++ & 0x3f;		/* make sure 24 hr */
		clock->date = *ptr++;
		clock->month = *ptr++;
		clock->year = *ptr++;
		clock->day = *ptr++;
	}

	clock->command |= WTR_TE;			/* thaw external */

	splx(s);
}

static void
setbadclock(void)
{
	register ds1286_clk_t *clock = RTDS_CLOCK_ADDR;
	int s = splhi();
	clock->ram[RT_RAM_FLAGS] |= RT_FLAGS_INVALID;
	splx(s);
}

static void
clearbadclock(void)
{
	register ds1286_clk_t *clock = RTDS_CLOCK_ADDR;
	int s = splhi();
	clock->ram[RT_RAM_FLAGS] &= ~RT_FLAGS_INVALID;
	splx(s);
}

static int
isbadclock(void)
{
	register ds1286_clk_t *clock = RTDS_CLOCK_ADDR;
	int rc, s = splhi();
	rc = clock->ram[RT_RAM_FLAGS] & RT_FLAGS_INVALID;
	splx(s);
	return(rc);
}

/*
 * Called from prom_reinit(), in case syssgi(SET_AUTOPWRON) was done.
 * If time of the day alarm vars are to be set then program
 * RTC to power system back on using time-of-day alarm, unless the
 * current time is already past that value.
*/
void
set_autopoweron(void)
{
	register ds1286_clk_t *clock = (ds1286_clk_t *) RTDS_CLOCK_ADDR;
	int month, day, hours, mins, year, day_of_week, command;
	extern time_t autopoweron_alarm;
	long year_secs;

	if((unsigned long)time > (unsigned long)autopoweron_alarm)
		return;	/* not requested, or requested time is already past */

	year_secs = autopoweron_alarm;

	/*
	 * calculate the day of the week 
	 */
	day_of_week = get_dayof_week(year_secs);
	/*
	 * Whittle the time down to an offset in the current year, by
	 * subtracting off whole years as long as possible. 
	 */
	year = YRREF;
	for (;;) {
		register secyr = SECYR;
		if (LEAPYEAR(year))
			secyr += SECDAY;
		if (year_secs < secyr)
			break;
		year_secs -= secyr;
		year++;
	}
	/*
	 * Break seconds in year into month, day, hours, minutes,
	 * seconds 
	 */
	if (LEAPYEAR(year))
		month_days[1]++;
	for (month = 0;
	     year_secs >= month_days[month] * SECDAY;
	     year_secs -= month_days[month++] * SECDAY)
		continue;
	month++;
	if (LEAPYEAR(year))
		month_days[1]--;

	for (day = 1; year_secs >= SECDAY; day++, year_secs -= SECDAY)
		continue;

	for (hours = 0; year_secs >= SECHOUR; hours++, year_secs -= SECHOUR)
		continue;

	for (mins = 0; year_secs >= SECMIN; mins++, year_secs -= SECMIN)
		continue;

	if (!((mins > 59) || (hours > 23) || (day_of_week > 7))) {
		/* Set alarm to activate when minutes, hour and day match.
		 */
		clock->min_alarm = (char) (((mins / 10) << 4) | (mins % 10));
		clock->hour_alarm = (char) (((hours / 10) << 4) | (hours % 10));
		clock->day_alarm = (char) (((day_of_week / 10) << 4) |
					    (day_of_week % 10));

		command = clock->command;
		command |= WTR_TIME_DAY_INTA;		/* Enable INTA */
		command &= ~WTR_DEAC_TDM;		/* Activate TDM */
		clock->command = command;
	}
}

/*
 * converts year_secs to the day of the week 
 * (i.e. Sun = 1 and Sat = 7)
 */
static int
get_dayof_week(time_t year_secs)
{
        int days,yr=YRREF;
        int year_day;
#define EXTRA_DAYS      3		/* used to get day of week */

        days = 0;
        for (;;) {
                register secyear = SECYR;
                if (LEAPYEAR(yr))
                        secyear += SECDAY;

                if (year_secs >=secyear)
                        year_day = secyear;
                else
                        year_day = year_secs;
                for (;;) {
                        if (year_day < SECDAY)
                                break;

                        year_day -= SECDAY;
                        days++;
                }

                if (year_secs < secyear)
                        break;
                year_secs -= secyear;
                yr++;
        }

        if (days > EXTRA_DAYS) {
                days -=EXTRA_DAYS;
                days %= 7;
                days %= 7;
        } else
                days +=EXTRA_DAYS+1;
        return(days+1);

}

/* END of Time Of Day clock chip stuff */

/*	called from  clock() periodically to see if system time is 
	drifting from the real time clock chip. Seems to take about
	about 30 seconds to make a 1 second adjustment, judging
	by my debug printfs.  Only do if different by 2 seconds or more.
	1 second difference often occurs due to slight differences between
	kernel and chip, 2 to play safe.
	Main culprit seems to be graphics stuff keeping interrupts off for
	'long' times; it's easy to lose a tick when they are happening every
	milli-second.  Olson, 8/88
*/
void
chktimedrift(void)
{
	long todtimediff;

	todtimediff = rtodc() - time;
	if((todtimediff > 2) || (todtimediff < -2))
		(void)doadjtime(todtimediff * USEC_PER_SEC);
}

/*	Fetch the nvram table from the PROM.
	The table is static because we can't call kern_malloc at this point,
	and allocating whole pages would be wasteful.  We can at least tell
	if the table is larger than we think it is by the return value.
*/

/* the actual size of the table, including the null entry at the end
	is 29 members as of 8/93 */
#define NVRAMTAB 30
static struct nvram_entry nvram_tab[NVRAMTAB];
static int use_dallas;
static int set_dallas;		/* XXX can we move setting to mlreset()? */

void
load_nvram_tab(void)
{
#if !_K64PROM32
	arcs_nvram_tab((char *)nvram_tab, sizeof(nvram_tab));
#else
	struct nvram_entry32 ne32[NVRAMTAB];
	int i;

	arcs_nvram_tab((char *)ne32, sizeof(nvram_tab));

	for (i=0; i < NVRAMTAB; i++) {
		bcopy(ne32[i].nt_name,nvram_tab[i].nt_name,MAXNVNAMELEN);
		nvram_tab[i].nt_value = 0;
		nvram_tab[i].nt_nvaddr = ne32[i].nt_nvaddr;
		nvram_tab[i].nt_nvlen = ne32[i].nt_nvlen;
	}
#endif
}

/*	START of NVRAM stuff.
 */

/*
 * assign different values to cpu_auxctl to access different NVRAMs, one on
 * the backplane, two possible others on optional GIO ethernet cards
 *
 * XXX: the *_word() variants are provided to read R4000 EEROM on MC since
 *      MC only seems to respond to word writes on that register.
 */
volatile unchar *cpu_auxctl;
volatile uint *cpu_auxctl_word;
#define	CPU_AUXCTL	cpu_auxctl

/*
 * enable the serial memory by setting the console chip select
 */
static void
nvram_cs_on(void)
{
	*CPU_AUXCTL &= ~CPU_TO_SER;
	*CPU_AUXCTL &= ~SERCLK;
	*CPU_AUXCTL &= ~NVRAM_PRE;
	DELAY(1);
	*CPU_AUXCTL |= CONSOLE_CS;
	*CPU_AUXCTL |= SERCLK;
}

#ifdef R4600SC
/*
 * enable the serial memory by setting the console chip select
 */
static void
nvram_cs_on_word(void)
{
	*cpu_auxctl_word &= ~CPU_TO_SER;
	*cpu_auxctl_word &= ~SERCLK;
	*cpu_auxctl_word &= ~NVRAM_PRE;
	DELAY(1);
	*cpu_auxctl_word |= CONSOLE_CS;
	*cpu_auxctl_word |= SERCLK;
}
#endif

/*
 * turn off the chip select
 */
static void
nvram_cs_off(void)
{
	*CPU_AUXCTL &= ~SERCLK;
	*CPU_AUXCTL &= ~CONSOLE_CS;
	*CPU_AUXCTL |= NVRAM_PRE;
	*CPU_AUXCTL |= SERCLK;
}

#ifdef R4600SC
static void
nvram_cs_off_word(void)
{
	*cpu_auxctl_word &= ~SERCLK;
	*cpu_auxctl_word &= ~CONSOLE_CS;
	*cpu_auxctl_word |= NVRAM_PRE;
	*cpu_auxctl_word |= SERCLK;
}
#endif

#define	BITS_IN_COMMAND	11
/*
 * clock in the nvram command and the register number.  For the
 * natl semi conducter nv ram chip the op code is 3 bits and
 * the address is 6/8 bits. 
 */
static void
nvram_cmd(uint cmd, uint reg)
{
	ushort ser_cmd;
	int i;

	ser_cmd = cmd | (reg << (16 - BITS_IN_COMMAND));
	for (i = 0; i < BITS_IN_COMMAND; i++) {
		if (ser_cmd & 0x8000)	/* if high order bit set */
			*CPU_AUXCTL |= CPU_TO_SER;
		else
			*CPU_AUXCTL &= ~CPU_TO_SER;
		*CPU_AUXCTL &= ~SERCLK;
		*CPU_AUXCTL |= SERCLK;
		ser_cmd <<= 1;
	}
	*CPU_AUXCTL &= ~CPU_TO_SER;	/* see data sheet timing diagram */
}

#ifdef R4600SC
static void
nvram_cmd_word(uint cmd, uint reg)
{
	ushort ser_cmd;
	int i;

	ser_cmd = cmd | (reg << (16 - BITS_IN_COMMAND));
	for (i = 0; i < BITS_IN_COMMAND; i++) {
		if (ser_cmd & 0x8000)	/* if high order bit set */
			*cpu_auxctl_word |= CPU_TO_SER;
		else
			*cpu_auxctl_word &= ~CPU_TO_SER;
		*cpu_auxctl_word &= ~SERCLK;
		*cpu_auxctl_word |= SERCLK;
		ser_cmd <<= 1;
	}
	*cpu_auxctl_word &= ~CPU_TO_SER; /* see data sheet timing diagram */
}
#endif

/*
 * after write/erase commands, we must wait for the command to complete
 * write cycle time is 10 ms max (~5 ms nom); we timeout after ~20 ms
 *    NVDELAY_TIME * NVDELAY_LIMIT = 20 ms
 */
#define NVDELAY_TIME	100	/* 100 us delay times */
#define NVDELAY_LIMIT	200	/* 200 delay limit */

static
nvram_hold(void)
{
	int error = 0;
	int timeout = NVDELAY_LIMIT;

	nvram_cs_on();

	while (!(*CPU_AUXCTL & SER_TO_CPU) && timeout--)
		DELAY(NVDELAY_TIME);

	if (!(*CPU_AUXCTL & SER_TO_CPU))
		error = -1;
	nvram_cs_off();
	return (error);
}



/*
 * get_nvreg -- read a 16 bit register from non-volatile memory.  Bytes
 *  are stored in this string in big-endian order in each 16 bit word.
 */
ushort
get_nvreg(int nv_regnum)
{
	ushort ser_read = 0;
	int i;

	if (!set_dallas) {
		set_dallas = 1;
		use_dallas = !is_fullhouse();
	}

	/* Need both checks because optional GIO cards have onboard EEPROM */
	if (use_dallas  && cpu_auxctl == (volatile unchar *)PHYS_TO_K1(CPU_AUX_CONTROL)) {
		uint *nvramptr = (uint *)NVRAM_ADRS(nv_regnum * 2);
		ser_read = *nvramptr++ & 0xff;
		ser_read = (ser_read << 8) | *nvramptr & 0xff;
#ifdef R4600SC
	} else if (cpu_auxctl == (volatile unchar *)PHYS_TO_K1(EEROM)) {
		cpu_auxctl_word = (volatile uint *)cpu_auxctl;
		*cpu_auxctl_word &= ~NVRAM_PRE;
		nvram_cs_on_word();		/* enable chip select */
		nvram_cmd_word(SER_READ, nv_regnum);

		/* clock the data ouf of serial mem */
		for (i = 0; i < 16; i++) {
			*cpu_auxctl_word &= ~SERCLK;
			DELAY(1);
			*cpu_auxctl_word |= SERCLK;
			DELAY(1);
			ser_read <<= 1;
			ser_read |= (*cpu_auxctl_word & SER_TO_CPU) ? 1 : 0;
		}

		nvram_cs_off_word();
#endif /* R4600SC */
	} else {
		*CPU_AUXCTL &= ~NVRAM_PRE;
		nvram_cs_on();			/* enable chip select */
		nvram_cmd(SER_READ, nv_regnum);

		/* clock the data ouf of serial mem */
		for (i = 0; i < 16; i++) {
			*CPU_AUXCTL &= ~SERCLK;
			*CPU_AUXCTL |= SERCLK;
			ser_read <<= 1;
			ser_read |= (*CPU_AUXCTL & SER_TO_CPU) ? 1 : 0;
		}
		
		nvram_cs_off();
	}

	return(ser_read);
}


/*
 * set_nvreg -- writes a 16 bit word into non-volatile memory.  Bytes
 *  are stored in this register in big-endian order in each 16 bit word.
 */
static int
set_nvreg(int nv_regnum, unsigned short val)
{
	int error;
	int i;

	if (!set_dallas) {
		set_dallas = 1;
		use_dallas = !is_fullhouse();
	}

	/* Need both checks because optional GIO cards have onboard EEPROM */
	if (use_dallas  && cpu_auxctl == (volatile unchar *)PHYS_TO_K1(CPU_AUX_CONTROL)) {
		uint *nvramptr = (uint *)NVRAM_ADRS(nv_regnum * 2);
		*nvramptr++ = val >> 8;
		*nvramptr = val & 0xff;
		error = 0;
	} else {
		*CPU_AUXCTL &= ~NVRAM_PRE;
		nvram_cs_on();
		nvram_cmd(SER_WEN, 0);	
		nvram_cs_off();

		nvram_cs_on();
		nvram_cmd(SER_WRITE, nv_regnum);

		/*
		 * clock the data into serial mem 
		 */
		for (i = 0; i < 16; i++) {
			if (val & 0x8000)	/* pull the bit out of high order pos */
				*CPU_AUXCTL |= CPU_TO_SER;
			else
				*CPU_AUXCTL &= ~CPU_TO_SER;
			*CPU_AUXCTL &= ~SERCLK;
			*CPU_AUXCTL |= SERCLK;
			val <<= 1;
		}
		*CPU_AUXCTL &= ~CPU_TO_SER;	/* see data sheet timing diagram */
		
		nvram_cs_off();
		error = nvram_hold();

		nvram_cs_on();
		nvram_cmd(SER_WDS, 0);
		nvram_cs_off();
	}

	return (error);
}

static char
nvchecksum(void)
{
	register signed char checksum;
	register int nv_reg;
	ushort nv_val;

    /*
     * Seed the checksum so all-zeroes (all-ones) nvram doesn't have a zero
     * (all-ones) checksum.
     */
	checksum = 0xa5;

    /*
     * do the checksum on all of the nvram, skip the checksum byte !!
     */
	for(nv_reg = 0; nv_reg < NVLEN_MAX / 2; nv_reg++) {
		nv_val = get_nvreg(nv_reg);
		if(nv_reg == (NVOFF_CHECKSUM / 2))
#if NVOFF_CHECKSUM & 1
				checksum ^= nv_val >> 8;
#else
				checksum ^= nv_val & 0xff;
#endif
		else
			checksum ^= (nv_val >> 8) ^ (nv_val & 0xff);
		/* following is a tricky way to rotate */
		checksum = (checksum << 1) | (checksum < 0);
	}

	return (char)checksum;
}

/*
 * set_an_nvram -- write string to non-volatile memory
 */
static
set_an_nvram(register int nv_off, register int nv_len, register char *string)
{
	unsigned short curval;
	char checksum[2];
	int nv_off_save; 
	int i;

	nv_off_save = nv_off;

	if(nv_off % 2 == 1 && nv_len > 0) {
		curval  = get_nvreg(nv_off / 2);
		curval &= 0xff00;
		curval |= *string;
		if (set_nvreg(nv_off / 2, curval))
			return (-1);
		string++;
		nv_off++;
		nv_len--;
	}

	for (i = 0; i < nv_len / 2; i++) {
		curval = (unsigned short) *string++ << 8;
		curval |= *string;
		string++;
		if (set_nvreg(nv_off / 2 + i, curval))
			return (-1);
	}

	if (nv_len % 2 == 1) {
		curval = get_nvreg((nv_off + nv_len) / 2);
		curval &= 0x00ff;
		curval |= (unsigned short) *string << 8;
		if (set_nvreg((nv_off + nv_len) / 2, curval))
			return (-1);
	}

	if (nv_off_save != NVOFF_CHECKSUM) {
		checksum[0] = nvchecksum();
		checksum[1] = 0;
		return (set_an_nvram(NVOFF_CHECKSUM, NVLEN_CHECKSUM, checksum));
	}
	else
		return (0);
}

#ifdef MFG_EADDR
/*
 * set_eaddr - set ethernet address in prom
 */
#define TOHEX(c) ((('0'<=(c))&&((c)<='9')) ? ((c)-'0') : ((c)-'a'+10))
int
set_eaddr (char *value)
{
	char digit[6], *cp;
	int dcount;

	/* expect value to be the address of an ethernet address string
	 * of the form xx:xx:xx:xx:xx:xx (lower case only)
	 */
	for (dcount = 0, cp = value; *cp ; ) {
	    if (*cp == ':') {
		cp++;
		continue;
	    } else if (!ISXDIGIT(*cp) || !ISXDIGIT(*(cp+1))) {
		return -1;
	    } else {
		if (dcount >= 6)
		    return -1;
		digit[dcount++] = (TOHEX(*cp)<<4) + TOHEX(*(cp+1));
		cp += 2;
	    }
	}

	cpu_auxctl = (volatile unchar *)PHYS_TO_K1(CPU_AUX_CONTROL);

	for (dcount = 0; dcount < NVLEN_ENET; ++dcount)
	    if (set_an_nvram(NVOFF_ENET+dcount, 1, &digit[dcount]))
		return -1;

	return 0;
}
#endif /* MFG_EADDR */


/* Set nv ram variable name to value
 *	return  0 - success, -1 on failure
 * All the real work is done in set_an_nvram().
 * There is no get_nvram because the code that would use
 * it always gets it from kopt_find instead.
 * This is called from syssgi().
 */
int
set_nvram(char *name, char *value)
{
	register struct nvram_entry *nvt;
	int valuelen = strlen(value);
	char _value[20];
	int _valuelen=0;

	if(!strcmp("eaddr", name))
#ifdef MFG_EADDR
	    return set_eaddr(value);
#else
	    return -2;
#endif /* MFG_EADDR */

	/* Don't allow setting the password from Unix, only clearing. */
	if (!strcmp(name, "passwd_key") && valuelen)
		return -2;

	/* change the netaddr to the nvram format */
	if (strcmp(name, "netaddr") == 0) {
		char buf[4];
		char *ptr = value;
		int i;

		strncpy(_value, value, 19);
		value[19] = '\0';
		_valuelen = valuelen;

		/* to the end of the string */
		while (*ptr)
			ptr++;

		/* convert string to number, one at a time */
		for (i = 3; i >= 0; i--) {
			while (*ptr != '.' && ptr >= value)
				ptr--;
			buf[i] = atoi(ptr + 1);
			if (ptr > value)
				*ptr = 0;
		}
		value[0] = buf[0];
		value[1] = buf[1];
		value[2] = buf[2];
		value[3] = buf[3];
		valuelen = 4;
	}

	cpu_auxctl = (volatile unchar *)PHYS_TO_K1(CPU_AUX_CONTROL);

	/* check to see if it is a valid nvram name */
	for(nvt = nvram_tab; nvt->nt_nvlen; nvt++)
	{
		if(!strcmp(nvt->nt_name, name)) {
			int status;

			if(valuelen > nvt->nt_nvlen)
			  return NULL;

			if(valuelen < nvt->nt_nvlen)
				++valuelen;	/* write out NULL */
			status = set_an_nvram(nvt->nt_nvaddr, valuelen, value);

			/* reset value if changed */
			if (_valuelen)
				strcpy(value, _value);

			return status;
		}
        }
	return -1;
}

/* END of NVRAM stuff */

void
init_all_devices(void)
{
	uphwg_init(hwgraph_root);
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

	for (i=0; i<bdevcnt; i++)
		bdevsw[i].d_cpulock = 0;

	for (i=0; i<cdevcnt; i++)
		cdevsw[i].d_cpulock = 0;
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

/* 
 * return board revision level (0-7)
 */
#define K1_SYS_ID	(volatile uint *)PHYS_TO_K1(HPC3_SYS_ID)
int
board_rev(void)
{

	return (*K1_SYS_ID & BOARD_REV_MASK) >> BOARD_REV_SHIFT;
}

static int
simpleisdnprobe (caddr_t addr)
{
    char rev_id;
    int revint;

    if (badaddr_val((char *)addr, sizeof(char), &rev_id)) {
	if (showconfig)
	    printf("isdn: probe failed\n");
	return -1;
    }

    revint = rev_id & 0x7;
    if (showconfig)
      printf("isdn: HSCX version id %d\n", revint);
    return(revint);
}

static int
simplevinoprobe (caddr_t addr)
{
    int rev_id;

    if (badaddr_val((char *)addr + 4, sizeof(int), &rev_id)) {
	if (showconfig)
	    printf ("vino: probe failed\n");
	return -1;
    }

    if (VINO_ID_VALUE(rev_id)  != VINO_CHIP_ID) {
	if (showconfig)
	    printf ("vino: probe found controller that's not VINO\n");
	return -1;
    }
    else {
	if (showconfig)
	    printf ("vino: rev %d\n", VINO_REV_NUM(rev_id));
	return (VINO_REV_NUM(rev_id));
    }
}

void
add_ioboard(void)	/* should now be add_ioboards() */
{
	if (is_fullhouse())
		add_to_inventory(INV_BUS,INV_BUS_EISA,0,0,4);

	/* We need this probe here so we can probe for VINO h/w before
	 * the vino driver is present.
	 */
	if (!is_fullhouse())
	{
	    /* read version id in both channels as a sanity check */
	    int vers = simpleisdnprobe((caddr_t)PHYS_TO_K1(HSCX_VSTR_B1REG));
	    if (vers >= 0 &&
		vers == simpleisdnprobe((caddr_t)PHYS_TO_K1(HSCX_VSTR_B2REG)))
	        /* We only use the Siemens controller and it's always on 
		 * the motherboard.  If we ever add others, we'll obviously 
		 * need a test here -wsm7/19/94. 
		 */
	        add_to_inventory(INV_NETWORK, INV_NET_ISDN_BRI,
				 INV_ISDN_SM, 0, (1<<24)|(1<<8)|0);
	      
	    if (simplevinoprobe ((caddr_t)PHYS_TO_K1(VINO_PHYS_BASE1)) >= 0)

		/* add only if not previously added by vino_init */
		if (!find_inventory(0, INV_VIDEO, INV_VIDEO_VINO, 0, 0,-1))

		    /* note that we indicate there's no software installed. */
		    add_to_inventory(
			INV_VIDEO, INV_VIDEO_VINO, 0, 0, INV_VINO_INDY_NOSW); 
	}
}

int ip22_bufpio_adjustment = 0;
char *cpu_rev_find( int, int *, int * );

void
add_cpuboard(void)
{
	int maj, min;

	add_to_inventory(INV_SERIAL, INV_ONBOARD, 0, 0, 2);

	add_to_inventory(INV_PROCESSOR, INV_CPUCHIP, cpuid(),0,
						private.p_cputype_word);
	/*
	 * PM5 has a fifo buffering the PIO, depth set to 3.
	 */
	if (IS_PM5(maj, min))
		ip22_bufpio_adjustment = 3;

	add_to_inventory(INV_PROCESSOR, INV_FPUCHIP, cpuid(),0,
						private.p_fputype_word);
	/* for type INV_CPUBOARD, ctrl is CPU board type, unit = -1 */
	/* Double the hinv cpu frequency for R4000s */
	add_to_inventory(INV_PROCESSOR,INV_CPUBOARD,
			 cpu_mhz_rating(), -1, INV_IP22BOARD);

#ifdef JUMP_WAR 
	/* Don't do the jump workaround for rev 3 or greater parts to reduce
	 * overhead.  Still has assorted branches, etc. in various places,
	 * but we still have some 2.2 parts, so we can't compile the kernel
	 * without JUMP_WAR.
	 */
	if ((((private.p_cputype_word & C0_IMPMASK) >> C0_IMPSHIFT) ==
	     C0_IMP_R4000) &&
	    (((private.p_cputype_word & C0_MAJREVMASK) >> C0_MAJREVSHIFT) >= 3)) {
		extern int R4000_jump_war_correct;
		R4000_jump_war_correct = 0;
	}
#endif

#define	IS_USING_16M_DRAM(x)	(((x) & MEMCFG_DRAM_MASK) >= MEMCFG_64MRAM)

	/*
	 * 16M DRAM part requires the rev C MC to function correctly
	 * note: cannot do this in mlreset() since it is too early to use
	 * cmn_err() there
	 */
	if (mc_rev_level < 3) {
		if (IS_USING_16M_DRAM(memconfig_0)
		    || IS_USING_16M_DRAM(memconfig_0 >> 16)
		    || IS_USING_16M_DRAM(memconfig_1)
		    || IS_USING_16M_DRAM(memconfig_1 >> 16)) {
			cmn_err(CE_WARN, "This system may require the revision C Memory Controller (MC) chip\n"
			"\tin order to operate correctly with the type of memory SIMMs installed.\n"
			"\tYou do not need the new MC unless you experience memory errors.");
		}
	}
}

int
cpuboard(void)
{
	return INV_IP22BOARD;
}

#if _K64PROM32
/* if start out with a PROM32, then SBP is 32-bit, so need
 * to change it right away to 64-bit flavor SPB
 */
void
swizzle_SPB()
{
	if(SPB->Signature != SPBMAGIC) {
	__int32_t f1, f2, f3, f4;
		f1 = SPB32->TransferVector;
		f2 = SPB32->TVLength;
		f3 = SPB32->PrivateVector;
		f4 = SPB32->PTVLength;

		SPB->Signature = SPBMAGIC;
		SPB->Length = sizeof(spb_t);
		SPB->Version = SGI_ARCS_VERS;
		SPB->Revision = SGI_ARCS_REV;
		SPB->RestartBlock = 0;
		SPB->DebugBlock = 0;
		SPB->AdapterCount = 0;

		SPB->TransferVector = (FirmwareVector *)PHYS_TO_K1(f1 & 0x1fffffff);
		SPB->TVLength = f2;
		SPB->PrivateVector = (long *)PHYS_TO_K1(f3 & 0x1fffffff);
		SPB->PTVLength = f4;

		/* leave SPB->GEVector and utlb miss vector random for now */
	}
}
#endif

/*
 * On IP22, unix overlays the prom's bss area.  Therefore, only
 * those prom entry points that do not use prom bss or are 
 * destructive (i.e. reboot) may be called.  There is however,
 * a period of time before the prom bss is overlayed in which
 * the prom may be called.  After unix wipes out the prom bss
 * and before the console is inited, there is a very small window
 * in which calls to dprintf will cause the system to hang.
 *
 * If symmon is being used, it will plug a pointer to its printf
 * routine into the restart block which the kernel will then use
 * for dprintfs.  The advantage of this is that the kp command
 * will not interfere with the kernel's own print buffers.
 *
 */
# define ARGS	a0,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15
void
dprintf(fmt,ARGS)
char *fmt;
long ARGS;
{
	/*
	 * Check for presence of symmon
	 */
	if ( SPB->DebugBlock && ((db_t *)SPB->DebugBlock)->db_printf ) {
		(*((db_t *)SPB->DebugBlock)->db_printf)(fmt, ARGS);
	} else {
		/*
		 * cn_is_inited() implies that PROM bss has been wiped out
		 */
		if (cn_is_inited())
			cmn_err(CE_CONT,fmt,ARGS);
		else {
			/*
			 * try printing through the prom
			 */
			arcs_printf (fmt,ARGS);
		}
	}
}

int
readadapters(uint_t bustype)
{
	switch (bustype) {
		
	case ADAP_SCSI:
		if (wd95cnt > wd93cnt)
			return wd95cnt;
		return wd93cnt;

	case ADAP_LOCAL:
		return(1);

	case ADAP_EISA:
		if (is_fullhouse())
			return(1);
		return(0);

	case ADAP_GIO:
		return(1);

	default:
		return(0);

	}
}

piomap_t*
pio_mapalloc (uint_t bus, uint_t adap, iospace_t* iospace, int flag, char *name)
{
	int	 i;
	piomap_t *piomap;

	if (bus < ADAP_GFX || bus > ADAP_EISA)
		return(0);

	/* Allocates a handle */

	piomap = (piomap_t*) kern_calloc(1, sizeof(piomap_t));

	/* fills the handle */

	piomap->pio_bus		= bus;
	piomap->pio_adap	= adap;
	piomap->pio_type	= iospace->ios_type;
	piomap->pio_iopaddr	= iospace->ios_iopaddr;
	piomap->pio_size	= iospace->ios_size;
	piomap->pio_flag	= flag;
	piomap->pio_reg		= PIOREG_NULL;
        for (i = 0; i < 7; i++)
		if (name[i])
			piomap->pio_name[i] = name[i];

	if (bus == ADAP_EISA)
		switch (iospace->ios_type) {

		    case PIOMAP_EISA_IO:
			piomap->pio_vaddr = (caddr_t)(EISAIO_TO_K1(iospace->ios_iopaddr));
			break;

		    case PIOMAP_EISA_MEM:
			piomap->pio_vaddr = (caddr_t)(EISAMEM_TO_K1(iospace->ios_iopaddr));
			eisa_refresh_on();
			break;

		    default:
			kern_free(piomap);
			return 0;
		}

	return(piomap);
}

caddr_t 
pio_mapaddr (piomap_t* piomap, iopaddr_t addr)
{
	switch(piomap->pio_bus){

	/* Change needed to support New form of VECTOR lines. */
	case ADAP_GIO:	
		return((caddr_t)(PHYS_TO_K1(addr)));

	default: 
		return piomap->pio_vaddr + (addr - piomap->pio_iopaddr);
	}
}

void
pio_mapfree (piomap_t* piomap)
{
	if (piomap)
		kern_free(piomap);
}

/* Compute the number of picosecs per cycle counter tick.
 *
 * Note that if the cpu clock does not divide nicely into 2000000,
 * the cycle count will not be exact.  (Random samples of possible
 * clock rates show the difference will be less than .003% of the
 * exact cycle count).
 */
__psint_t
query_cyclecntr(uint *cycle)
{
	static int sysclk_divider[] = {2, 3, 4, 6, 8};
	unsigned int ec = (get_r4k_config() & CONFIG_EC) >> 28;
	int maj, min;
	int ust_cpufreq = private.ust_cpufreq;

	/*
	 * The IP22 PM5 module is a 250 MHz R4400 which uses
	 * the divide-by-4, plus an external PAL to synchronize
	 * it with a 50 MHz SysAD. So, if we're a 250MHz
	 * R4400, a divisor of "4" is really a divisor of "5".
	 */
	if (IS_PM5(maj, min))
		sysclk_divider[2] = 5;

	/*
	 * 50Mhz was supposed to the fundamental SysAD freq
	 * that the MC's RPSS counter would based
	 * on, but due to different CPU speeds 50Mhz
	 * could not always be used.
	 * IP22's will used the RPSS_DIVIDER value of 2
	 * making RPSS_COUNTER freq to be sysAD/2.
	 * due to the crystals used for PM there is a
	 * difference between the marketing freq and the
	 * actual freq for 133 and 175 MHz PMs and between
	 * I2 and INDY's.
	 * And in turn the RPSS counter ticks
	 * (which was supposed to be a fundemental constant
	 * in the system) are different.
	 * We hope the the crystal freq for the non-50Mhz
	 * parts will be same in the shipped units.
	 * 
	 * ----------+--------+--------+-------
	 * PM type    crystal   cpu	counter
	 *              MHz     MHz     nano-secs
	 * ----------+--------+--------+-------
	 * I2   133     66.600  133.2   45.045
	 * ----------+--------+--------+-------
	 * I2   175     ????    175.0   45.714
	 * ----------+--------+--------+-------
	 * INDY 133     66.666  133.332 45.000
	 * ----------+--------+--------+-------
	 * INDY 175     29.123  174.738 45.782
	 * ----------+--------+--------+-------
	 * other IP22	50.000  50 * X  40.000
	 * ----------+--------+--------+-------
	 */
#define IP22_133_I2_RPSS_TICK_PS	45045
#define IP22_133_INDY_RPSS_TICK_PS	45000
#define IP22_175_INDY_RPSS_TICK_PS	45782

	if (ust_cpufreq > 66000000 && ust_cpufreq < 67000000) {
	    if (is_fullhouse())
		*cycle = IP22_133_I2_RPSS_TICK_PS;
	    else
		*cycle = IP22_133_INDY_RPSS_TICK_PS;
	} else if (!is_fullhouse() &&
		   (ust_cpufreq > 87000000 && ust_cpufreq < 88000000))
	    *cycle = IP22_175_INDY_RPSS_TICK_PS;
	else 
	    *cycle = ((RPSS_DIV * 1000000 * sysclk_divider[ec]) /
		  ((private.ust_cpufreq + 250000) / 500000));
	return PHYS_TO_K1(RPSS_CTR);
}
/*
 * This routine returns the value of the cycle counter. It needs to
 * be the same counter that the user sees when mapping in the CC
 */
__int64_t
absolute_rtc_current_time()
{
	unsigned int time;
	time =  *(unsigned int *) PHYS_TO_K1(RPSS_CTR);
	return((__int64_t) time);
}


/* Keyboard bell using EIU 8254 timer.  This is only for Fullhouse.
 */
#define MAXFREQ		12500
#define FQVAL(v)	(1193000/(v))
#define FQVAL_MSB(v)	(((v)&0xff00)>>8)
#define FQVAL_LSB(v)	((v)&0xff)

#define MAXBELLQ	32			/* must be a power of 2 */
#define pckbd_qnext(X)	(((X)+1) & (~MAXBELLQ))

#define INB(x)		(*(volatile u_char *)(EISAIO_TO_K1(x)))
#define OUTB(x,y)	(*(volatile u_char *)(EISAIO_TO_K1(x)) = (y))

struct {
	unsigned short pitch;
	unsigned short duration;
} bellq[MAXBELLQ];

static void start_bell(int pitch, int duration, int notimeout);

static struct {
	int head;			/* head of bellq */
	int tail;			/* tail of bellq */
	int pitch;			/* current pitch */
	toid_t tid;			/* timeout id */
	int flags;
#define BELLASLEEP	0x01
} bell;

int bellok;				/* needed by csu.s */

void
kbdbell_init(void)
{
	unsigned char tmp;

	/* No bell if on Guinness or no EIU */
	if (!is_fullhouse() ||
	    badaddr((volatile void *)EISAIO_TO_K1(IntTimer1CommandMode),1))
		return;

	OUTB(IntTimer1CommandMode, (EISA_CTL_SQ|EISA_CTL_LM|EISA_CTL_C2));
	tmp = INB(NMIStatus);
	OUTB(NMIStatus,tmp|EISA_C2_ENB);

	
	bell.flags = bell.head = bell.tail = 0;
	bellok = 1;
}

static void
quiet_bell(void)
{
	unsigned char tmp;

	/* turn off speaker. */
	tmp = INB(NMIStatus);
	tmp &= ~EISA_SPK_ENB;
	OUTB(NMIStatus,tmp);
}

/*ARGSUSED*/
static void
stop_bell(int arg)
{
	int s = splhi();

	quiet_bell();

	bell.tail = pckbd_qnext(bell.tail);
	if (bell.tail != bell.head) {
		start_bell(bellq[bell.tail].pitch,bellq[bell.tail].duration,0);
	}
	if (bell.flags & BELLASLEEP) {
		bell.flags &= ~BELLASLEEP;
		wakeup(bellq);
		splx(s);
		return;
	}
	splx(s);
}

static void
start_bell(int pitch, int duration, int notimeout)
{
	unsigned char tmp;
	int val;

	ASSERT(pitch);

	/* if changing the pitch, re-write hardware.
	 */
	if (pitch != bell.pitch) {
		bell.pitch = pitch;
		val = FQVAL(pitch);
		OUTB(IntTimer1SpeakerTone,FQVAL_LSB(val));
		flushbus();
		OUTB(IntTimer1SpeakerTone,FQVAL_MSB(val));
		flushbus();
	}

	/* approximate duration ms in 1/HZs, and enable generator.
	 */
	if (!notimeout)
		bell.tid = timeout(stop_bell,0,(duration*HZ)>>10);
	tmp = INB(NMIStatus);
	OUTB(NMIStatus,tmp|EISA_SPK_ENB);
}

/*ARGSUSED*/
int
pckbd_bell(int volume, int pitch, int duration)
{
	int last, new, s;
	unsigned int sum;

	if (!bellok || !pitch || !duration)
		return 0;

	if (pitch > MAXFREQ)
		pitch = MAXFREQ;

	s = splhi();

	/* Insure queue space, or sleep until space (or signal).
	 */
	if ((new=pckbd_qnext(bell.head)) == bell.tail) {
		bell.flags |= BELLASLEEP;
		if (sleep(bellq,PZERO)) {
			splx(s);
			return 0;
		}
	}

	/* If bell inactive, start it (and enqueue junk) else enqueue data.
	 */
	if (bell.head == bell.tail) {
		start_bell(pitch, duration, 0);
		bellq[bell.head].pitch = pitch;
	}
	else {
		/*  Try to coalesce with last entry, and handle back-off
		 * if coalese case, and if current bell matches frequency.
		 * We back off the duration since SGI keyboard bells dont
		 * seem to always ring if re-rung while on.
		 */
		last = bell.head - 1;
		if ((last >= 0) && (bellq[last].pitch == pitch)) {
			if (!(duration >>= 4)) duration = 1;
			if ((last != bell.tail)) {
				sum = bellq[last].duration + duration;
				if (sum < 0xffff) {
					bellq[last].duration = sum;
					goto done;
				}
			}
		}

		bellq[bell.head].duration = duration;
		bellq[bell.head].pitch = pitch;
	}

	bell.head = new;

done:
	splx(s);
	return(1);
}

/*  Make a quick beep to ack the power button press.  Override any
 * current bells temporarily.
 */
static void
power_bell(void)
{
	/*  Toss any current bell, and queue, then play a short beep.
	 * We need to explictly wait since switch debouncing and
	 * power double click make using timeout() impossible.
	 */
	if (bell.head != bell.tail)
		untimeout(bell.tid);
	bell.head = pckbd_qnext(bell.tail = 0);
	quiet_bell();
	start_bell(500, 0, 1);
	DELAY(50000);
	quiet_bell();
}


/* stubs for if_ec2.c that were in the duart for Indigo */
u_int enet_collision;
int enet_carrier_on() { return 1; }
void reset_enet_carrier() {}

#if DEBUG

#include "sys/immu.h"

uint tlbdropin(
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
	extern int findcpufreq_raw(void);

	timer_freq = findcpufreq_raw();
	timer_unit = NSEC_PER_SEC/timer_freq;
	timer_high_unit = timer_freq;	/* don't change this */
	timer_maxsec = TIMER_MAX/timer_freq;
}


static void
volumeintr(int * curlevelp)
{
	volatile uint *p = (uint *)PHYS_TO_K1(HPC3_PANEL);
	volatile u_char *q = K1_LIO_1_MASK_ADDR; 
	int newlevel;
	int flag = 0;
	static int mute = 0, inmutemode  = 0;

        if (!(*p & PANEL_VOLUME_UP_ACTIVE))
		flag |= VOLUMEINC;

	if (!(*p & PANEL_VOLUME_DOWN_ACTIVE))
		flag |= VOLUMEDEC;

        if (flag == VOLUMEINC || flag == VOLUMEDEC) {
		if (inmutemode && mute==0) {	
			/* if it's in the mute mode then
			   restore the original volume level back */
			* curlevelp = (* volumectrl_ptr)(0x3, inmutemode);
			inmutemode = 0;	/* release from mute mode */
                        mute = 2;	/* to avoid to fall into mute mode */
		} else { 
			/* otherwise, go to set the new volume level */
			newlevel = ((*curlevelp*5)+3) >> 2;
			* curlevelp = (* volumectrl_ptr)(flag, newlevel-*curlevelp);
			mute = 0;
		}
                timeout(volumeintr, curlevelp, 15);
		return;
	} else if (flag == VOLUMEMUTE) {
		/* delay one round to confirm the request */
		if (mute == 1) {
			* curlevelp = (* volumectrl_ptr)(flag, inmutemode);
			inmutemode ^= 1;
		}
		mute++;
                timeout(volumeintr, curlevelp, 15);
				return;
	}

	*p |= PANEL_VOLUME_UP_INT | PANEL_VOLUME_DOWN_INT
		| POWER_SUP_INHIBIT;
	*q |= LIO_POWER;
	mute = 0;
}

int
cpu_isvalid(int cpu)
{
	return !((cpu < 0) || (cpu >= maxcpus) || (pdaindr[cpu].CpuId == -1));
}

/*
 * This is just a stub on an IP22.
 */
/*ARGSUSED*/
void
debug_stop_all_cpus(void *stoplist)
{
	return;
}


/*  Hook to reset all the GIO slots before we call into the PROM.  This
 * makes sure nothing is dependent on memory when the PROM clear it.
 */
void
gioreset(void)
{
	volatile unsigned int *cfg = (unsigned int *)PHYS_TO_K1(CPU_CONFIG);
	*cfg &= ~CONFIG_GRRESET;
	flushbus();
	us_delay(5);
	*cfg |= CONFIG_GRRESET;
	flushbus();
}

/*
 *  The following functions allow for sharing GIO_SLOT_0 at
 *  GIO_INTERRUPT_1 between to compatible boards.  The ISR of each
 *  board is registered via setgiosharedvector(), which at interrupt
 *  time will result in a fanout function invoking each ISR.
 *
 *  If only one ISR is currently registered, then it is registered directly
 *  with GIO_INTERRUPT_1 at GIO_SLOT_0, otherwise if two are registered
 *  then sharedfanout() is registered via setgiovector() for later
 *  fanout.
 */

/*
 *  Keep ISR address and argument for both the upper and lower physical
 *  slots of physical GIO_SLOT_0.
 */
typedef struct shared_isr {
	void (*isr)(__psint_t, struct eframe_s *);
	__psint_t arg;
} shared_isr_t;

static shared_isr_t giofuncs[2];


/*
 *  Fanout function invoked for GIO_INTERRUPT_1, GIO_SLOT_0.
 */
/*ARGSUSED1*/
static void
sharedfanout(__psint_t arg, struct eframe_s *ep)
{

	if (giofuncs[GIO_SLOT_0_LOWER].isr)
		(*giofuncs[GIO_SLOT_0_LOWER].isr)
		 (giofuncs[GIO_SLOT_0_LOWER].arg, ep);

	if (giofuncs[GIO_SLOT_0_UPPER].isr)
		(*giofuncs[GIO_SLOT_0_UPPER].isr)
		 (giofuncs[GIO_SLOT_0_UPPER].arg, ep);

	return;

}  /* End of sharedfanout() */


/*
 *  ISR registration function for shared GIO slot 0.  It will introduce
 *  a fanout function if necessary in order to differentiate the ISRs
 *  of the boards sharing this slot.
 *
 *  Registration with the kernel is made using setgiovector() at
 *  GIO_INTERRUPT_1 for GIO_SLOT_0.
 */
void
setgiosharedvector(int position, void (*func)(__psint_t, struct eframe_s *),
 __psint_t arg)
{
	int s;
	int other;

	if (position < 0 || position > GIO_SLOT_0_UPPER) {
		cmn_err(CE_WARN,
		 "setgiosharedvector: position out of range (%d)\n",
		 position);
		return;
	}

	/*
	 *  Can't be used with Indy.
	 */
	if (!is_fullhouse()) {
		cmn_err(CE_WARN,
		 "setgiosharedvector: Improper system type - usage ignored\n");
		return;
	}

	other = (position == GIO_SLOT_0_LOWER ? GIO_SLOT_0_UPPER :
	 GIO_SLOT_0_LOWER);

	s = splgio1();

	giofuncs[position].isr = func;
	giofuncs[position].arg = arg;

	/*
	 *  If non-NULL, then an ISR is to be registered.
	 */
	if (func) {

		/*
		 *  If something has already been registered for the other
		 *  slot position, then we must register the fanout function.
		 */
		if (giofuncs[other].isr)
			setgiovector(GIO_INTERRUPT_1, GIO_SLOT_0, sharedfanout,
			 arg);

		/*
		 *  Otherwise, directly register the one ISR associated with
		 *  the slot.
		 */
		else
			setgiovector(GIO_INTERRUPT_1, GIO_SLOT_0, func, arg);
	}

	/*
	 *  De-register the ISR for the position.
	 */
	else {

		/*
		 *  If something has been registered for the other position,
		 *  then register it directly, effectively unfanning out.
		 */
		if (giofuncs[other].isr)
			setgiovector(GIO_INTERRUPT_1, GIO_SLOT_0,
			 giofuncs[other].isr, giofuncs[other].arg);

		/*
		 *  Otherwise, de-register the one ISR associated with
		 *  the slot.
		 */
		else
			setgiovector(GIO_INTERRUPT_1, GIO_SLOT_0, func, arg);
	}

	splx(s);
	return;

}  /* End of setgiosharedvector() */

int
is_thd(int level)
{
int ret;
	switch(level) {

	/* Threaded */
	case VECTOR_SCSI:
	case VECTOR_SCSI1:
	case VECTOR_PLP:
	case VECTOR_GIO1:
	case VECTOR_HPCDMA:
	case VECTOR_GIO2:
	case VECTOR_DUART:
	case VECTOR_VIDEO:
	case VECTOR_KBDMS:
	case VECTOR_GIOEXP0:
	case VECTOR_GIOEXP1:
	case VECTOR_ENET:
	case VECTOR_EISA:
		ret = 1;
		break;

	/* Not threaded */
	case VECTOR_GIO0:
	case VECTOR_LCL2:
	case VECTOR_POWER:
	case VECTOR_LCL3:
	case VECTOR_ACFAIL:

	/* Untested as threads */
	case VECTOR_GDMA:
	case VECTOR_ISDN_ISAC:
	case VECTOR_ISDN_HSCX:

	/* ? */
	default:
		ret = 0;
		break;
	} /* switch level */
 return ret;
}

/*
 * lcl_intrd
 * This routine is the base point for lcl interrupts which run as threads.
 * The ipsema() call is a bit magical, since it is not really a
 * semaphore.  Every time it is called, the thread running it restarts
 * itself at the beginning.
 */
void
lcl_intrd(lclvec_t *l)
{
	volatile unchar *int_mask;
	int s;

#ifdef ITHREAD_LATENCY
	xthread_update_latstats(l->lcl_lat);
#endif
	
	l->isr(l->arg, NULL);
	if ((l->lcl_flags&THD_HWDEP) == 0) {
		int_mask = K1_LIO_0_MASK_ADDR;
	} else if ((l->lcl_flags&THD_HWDEP) == 1) {
		int_mask = K1_LIO_1_MASK_ADDR;
	} else {
		int_mask = K1_LIO_2_MASK_ADDR;
	}

	s = disableintr();
	*int_mask |= (l->bit);
	enableintr(s);

	/* Wait for next interrupt */
	ipsema(&l->lcl_isync);	/* Thread always restarts at top. */
}

void
lcl_intrd_init(lclvec_t *lvp)
{
	xthread_set_func(KT_TO_XT(curthreadp), (xt_func_t *)lcl_intrd, lvp);
	atomicSetInt(&lvp->lcl_flags, THD_INIT);

	/* The ipsema will always come out in lcl_intrd, now.
	 * It will sleep if the semaphore has not been signalled.
	 */
	ipsema(&lvp->lcl_isync);
	/* NOTREACHED */
}

/*
 * lclvec_thread_setup -
 * this routine handles thread setup for the given lclvec entry
 */

static void
lclvec_thread_setup(int level, int lclid, lclvec_t *lvp)
{
	char thread_name[32];
	int pri;

	sprintf(thread_name, "lclintr%d.%d", lclid, level+1);

	/* XXX TODO - pri calculation/band checking */
	pri = (250 - lclid);
	xthread_setup(thread_name, pri, &lvp->lcl_tinfo, 
			(xt_func_t *)lcl_intrd_init, (void *)lvp);
}

/*
 * enable_ithreads -
 * scan the lclvec tables and setup any register interrupt threads
 * called out of main()
 */
void
enable_ithreads(void)
{
	lclvec_t *lvp;
	int lvl;

	ithreads_enabled = 1;
	for (lvl = 0; lvl < 8; lvl++) {
		lvp = &lcl0vec_tbl[lvl+1];
		if (lvp->lcl_flags & THD_REG) {
			ASSERT ((lvp->lcl_flags & THD_ISTHREAD));
			ASSERT (!(lvp->lcl_flags & THD_OK));
			lclvec_thread_setup(lvl, 0, lvp);
		}
	}
	for (lvl = 0; lvl < 8; lvl++) {
		lvp = &lcl1vec_tbl[lvl+1];
		if (lvp->lcl_flags & THD_REG) {
			ASSERT ((lvp->lcl_flags & THD_ISTHREAD));
			ASSERT (!(lvp->lcl_flags & THD_OK));
			lclvec_thread_setup(lvl, 1, lvp);
		}
	}
	for (lvl = 0; lvl < 8; lvl++) {
		lvp = &lcl2vec_tbl[lvl+1];
		if (lvp->lcl_flags & THD_REG) {
			ASSERT ((lvp->lcl_flags & THD_ISTHREAD));
			ASSERT (!(lvp->lcl_flags & THD_OK));
			lclvec_thread_setup(lvl, 2, lvp);
		}
	}
		
}
