/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef __RACER_IP30_H__
#define __RACER_IP30_H__

#ident "$Revision: 1.83 $"

/*
 * IP30.h - IP30/speedracer header file
 */

/* Some general guidelines for the RACER header files
 * 		(IP30.h, xbow.h, heart.h, etc.):
 *
 *	64-bit compilation is assumed
 *	Little endian bit/word ordering is not addressed
 */

#define _ARCSPROM

#if LANGUAGE_C
#include <sys/types.h>
#endif /* LANGUAGE_C */

#if _KERNEL || _STANDALONE
#include <sys/RACER/heart.h>			/* heart chip */
#include <sys/PCI/bridge.h>			/* bridge chip */
#include <sys/PCI/ioc3.h>			/* ioc3 chip */
#include <sys/xtalk/xbow.h>			/* xbow chip */
#endif /* _KERNEL */

/* Xbow and Heart are really hardwired to widgets 0x0 and 0x8,
 * respectively.  Bridge defaults to widget 0xf so the bootprom 
 * shows up at 0x1fc00000, as expected by the cpu.
 */
#define XBOW_ID			XBOW_WIDGET_ID
#define HEART_ID		XBOW_PORT_8
#define BRIDGE_ID		XBOW_PORT_F

#define XBOW_BASE		MAIN_WIDGET(XBOW_ID)
#define	XBOW_K1PTR		((xbow_t *)K1_MAIN_WIDGET(XBOW_ID))

#define HEART_BASE		MAIN_WIDGET(HEART_ID)
#define	HEART_CFG_K1PTR		((heart_cfg_t *)K1_MAIN_WIDGET(HEART_ID))

/* Actually, we could have more than one bridge... */
/* BRIDGE_BASE and BRIDGE_K1PTR refer to the IP30 BaseIO bridge. */
#define BRIDGE_BASE		MAIN_WIDGET(BRIDGE_ID)
#define	BRIDGE_K1PTR		((bridge_t *)K1_MAIN_WIDGET(BRIDGE_ID))

/* "slot" assignments for various devices
 * on the IP30 BaseIO Bridge.
 */
#define	BRIDGE_SCSI0_ID		0
#define	BRIDGE_SCSI1_ID		1
#define	BRIDGE_IOC3_ETH_ID	2
#define	BRIDGE_RAD_ID		3	/* does not use INTA (int3) */	
#define	BRIDGE_IOC3_SPKM_ID	4	/* does not use CFG SPACE */
#define BRIDGE_PASSWD		5	/* prom password jumper */
#define BRIDGE_POWER		6	/* soft power switch */
#define BRIDGE_ACFAIL		7	/* AC fail signal from the supply */

#define	BRIDGE_IOC3_ID 		BRIDGE_IOC3_ETH_ID /* ID used for CFG space */

#define	SCSI0_PCI_CFG_BASE	(BRIDGE_BASE+BRIDGE_TYPE0_CFG_DEV(BRIDGE_SCSI0_ID))
#define	SCSI1_PCI_CFG_BASE	(BRIDGE_BASE+BRIDGE_TYPE0_CFG_DEV(BRIDGE_SCSI1_ID))
#define	IOC3_PCI_CFG_BASE	(BRIDGE_BASE+BRIDGE_TYPE0_CFG_DEV(BRIDGE_IOC3_ID))
#define	RAD_PCI_CFG_BASE	(BRIDGE_BASE+BRIDGE_TYPE0_CFG_DEV(BRIDGE_RAD_ID))

#define	SCSI0_PCI_DEVIO_BASE	(BRIDGE_BASE+BRIDGE_DEVIO(BRIDGE_SCSI0_ID))
#define	SCSI1_PCI_DEVIO_BASE	(BRIDGE_BASE+BRIDGE_DEVIO(BRIDGE_SCSI1_ID))
#define	RAD_PCI_DEVIO_BASE	(BRIDGE_BASE+BRIDGE_DEVIO(BRIDGE_RAD_ID))
/*
 * do not use BRIDGE_DEVIO() macros which are used in assembly code
 */
#define	IOC3_PCI_DEVIO_BASE	(BRIDGE_BASE+BRIDGE_DEVIO2)

#define	IOC3_PCI_CFG_K1PTR	PHYS_TO_K1(IOC3_PCI_CFG_BASE)
#define	IOC3_PCI_DEVIO_K1PTR	PHYS_TO_K1(IOC3_PCI_DEVIO_BASE)

/*
 * Flash memory/prom base
 * 	base is at 0x1fc00000 in bridge MainIO(15) space and we run off of it
 *	and program (very carefully) it sparingly (until we have recovery prom).
 *	alternate base for 2nd flash is at + 2MB (0x1fe00000)
 *	mainly to program the flash
 *	using the 2nd bridge socket.
 */
#define FLASH_MEM_BASE		(BRIDGE_BASE+BRIDGE_EXTERNAL_FLASH)
#define FLASH_MEM_ALT_BASE	(FLASH_MEM_BASE + 0x00200000)

/*
 * IP30 preallocates certain Heart and Bridge interrupt
 * bits, beyond the normal Heart preallocations.
 */
#define	IP30_HVEC_WIDERR_XBOW	58	/* xbow errors to heart */
#define	IP30_HVEC_WIDERR_BASEIO	57	/* baseio bridge errors to heart */
#define IP30_HVEC_POWER		41	/* Power switch has been pressed */
#define IP30_HVEC_ACFAIL	IP30_HVEC_WIDERR_BASEIO
#define	IP30_HVEC_IOC3_ETHERNET	17	/* console dma done to heart */
#define	IP30_HVEC_IOC3_SERIAL	16	/* console dma done to heart */

#define	IP30_BVEC_IOC3_ETHERNET	BRIDGE_IOC3_ETH_ID
#define	IP30_BVEC_IOC3_SERIAL	BRIDGE_IOC3_SPKM_ID
#define IP30_BVEC_POWER		BRIDGE_POWER
#define IP30_BVEC_ACFAIL	BRIDGE_ACFAIL

/*
 * since we have more than enough PCI memory space for all the built in
 * devices, be generous and give 2 MB to each one of them
 */
#define	PCI_MAPPED_MEM_BASE(x)	((x) * 0x200000)

/* base address for the built in UARTs */
#define	IOC3_UART_A		(IOC3_PCI_DEVIO_BASE+IOC3_SIO_UA_BASE)
#define	IOC3_UART_B		(IOC3_PCI_DEVIO_BASE+IOC3_SIO_UB_BASE)

/*
 * the device is connected to the IOC3 generic I/O bus.  hardware
 * checks only the chip select (CS), any address within range is ok
 */
#if LANGUAGE_C
#define IP30_VOLTAGE_CTRL	(volatile uchar_t *) \
				(IOC3_PCI_DEVIO_K1PTR + IOC3_BYTEBUS_DEV0)
#else
#define IP30_VOLTAGE_CTRL	(IOC3_PCI_DEVIO_K1PTR + IOC3_BYTEBUS_DEV0)
#endif
#define CPU_MARGIN_LO			(0x1 << 0)
#define CPU_MARGIN_HI			(0x1 << 1)
#define VTERM_MARGIN_LO			(0x1 << 2)
#define VTERM_MARGIN_HI			(0x1 << 3)
#define PWR_SUPPLY_MARGIN_LO_NORMAL	(0x1 << 4)
#define PWR_SUPPLY_MARGIN_HI_NORMAL	(0x1 << 5)
#define FAN_SPEED_HI			(0x1 << 6)
#define FAN_SPEED_LO			(0x1 << 7)

/* Threshold of XIO thermal weights before turning on the fast fan.
 */
#define FAN_SPEED_HI_LOADS		3

/*
 * address of the address/data ports of the dallas ds1687 real time clock.
 * the device is connected to the IOC3 generic I/O bus.  hardware
 * checks only the chip select (CS), any address within range is ok
 */
#if LANGUAGE_C
#define	RTC_ADDR_PORT	(volatile uchar_t *) \
			(IOC3_PCI_DEVIO_K1PTR + IOC3_BYTEBUS_DEV1)
#define	RTC_DATA_PORT	(volatile uchar_t *) \
			(IOC3_PCI_DEVIO_K1PTR + IOC3_BYTEBUS_DEV2)
#else
#define	RTC_ADDR_PORT	(IOC3_PCI_DEVIO_K1PTR + IOC3_BYTEBUS_DEV1)
#define	RTC_DATA_PORT	(IOC3_PCI_DEVIO_K1PTR + IOC3_BYTEBUS_DEV2)
#endif

/* misc interrupts connected to the baseio bridge INT pins */
#define	PROM_PASSWD_DISABLE	(0x1 << BRIDGE_PASSWD)
#define	PWR_SWITCH_INTR		(0x1 << BRIDGE_POWER)
#define	PWR_SUPPLY_AC_FAIL_INTR	(0x1 << BRIDGE_ACFAIL)

/* crossbow widget interrupt vector (fixed as only one xbow per heart) */
#define	XBOW_VEC	58

/* LED color */
#define	LED_OFF		0
#define	LED_GREEN	1
#define	LED_AMBER	2

#if MP || _STANDALONE
#define MAXCPU	2			/* max is 4, but only 2 support now */
#else
#define MAXCPU	1
#endif

#define OSPL_SPDBG	0x00080000	/* spsema* debugging aid */

/* Mask to remove SW bits from pte. Note that the high-order
 * address bits are overloaded with SW bits, which limits the
 * physical addresses to 32 bits
 */
#define TLBLO_HWBITSHIFT	0		/* A shift value, for masking */

#define SYMMON_STACK_SIZE	0x2000
#define SYMMON_STACK_ADDR(x)	(PHYS_TO_K0_RAM(0x6000)+(x)*SYMMON_STACK_SIZE)
#define SYMMON_STACK		SYMMON_STACK_ADDR(0)

#define PROM_STACK              PHYS_TO_K1_RAM(0x1000000)
#define PROM_BSS                PHYS_TO_K1_RAM(0x0f00000)

#define PROM_CHILD_STACK        (PROM_BSS-8)

/*
 * for ECC error handler.  a pointer to a set of MAXCPU pointers to the ECC
 * exception handler stacks
 */
#define	CACHE_ERR_FRAMEPTR	(0x1000 - MAXCPU * 0x8)

#if defined(_STANDALONE)
#define	SR_PROMBASE		SR_CU0|SR_CU1|SR_KX|SR_FR
#define SR_POWER		SR_IBIT5

#if LANGUAGE_C			/* standalone prototypes */
char *ip30_ssram(void *, __uint64_t, __uint64_t, int);
char *ip30_tag_ssram(int);
__uint64_t ip30_ssram_swizzle(__uint64_t);
void ip30_power_switch_on(void);
void ip30_setup_power(void);
void ip30_disable_power(void);
int  ip30_checkintrs(__psunsigned_t, __psunsigned_t);
void powerspin(void);

void heart_warm_llp_reset(volatile heartreg_t*, heartreg_t, heartreg_t, volatile heartreg_t*);
void warm_reset_ip30_xio(void);
void init_ip30_chips(void);
void init_xbow_widget(xbow_t *,int,widget_cfg_t *);
void init_xbow(xbow_t *);
int  xtalk_probe(xbow_t *,int);

int launch_slave(int, void (*)(void *), void *, void (*)(void *), void *,
	         void *);

void pon_initio(void);
void pon_puthex64(__uint64_t);
void pon_puthex(__uint32_t);
void pon_puts(char *);
void pon_putc(char);

int ip30_addr_to_bank(__uint64_t);

ioc3reg_t print_ioc3_status(__psunsigned_t,int);
void print_bridge_status(bridge_t *,int);
heartreg_t print_heart_status(int);
void dump_heart_regs(void);
void dump_bridge_regs(bridge_t *);

void ip30_set_cpuled(int);
int cpu_probe_all(void);
int cpu_probe(void);

__psunsigned_t kv_to_bridge32_dirmap(void *pbridge, __psunsigned_t kvaddr);
__psunsigned_t bridge32_dirmap_to_phys(void *pbridge, __uint32_t daddr);
#endif
#endif	/* _STANDALONE */

#ifdef LANGUAGE_C			/* misc kernel prototypes */
void baseio_qlsave(vertex_hdl_t);
void cpu_soft_powerdown(void);
void set_autopoweron(void);
int pckbd_bell(int, int, int);

#ifdef _KERNEL
cpuid_t intr_spray_heuristic(device_desc_t);
int block_is_in_main_memory(iopaddr_t, size_t);
void ip30_intr_maint(void);

extern int pckbd_bell(int, int, int);
extern int is_octane_lx;
#endif

#ifdef HEART_COHERENCY_WAR
void heart_dcache_wb_inval(void *,int);
void heart_dcache_inval(void *,int);
void heart_write_dcache_wb_inval(void *,int);
void heart_write_dcache_inval(void *,int);
int heart_need_flush(int read);
#endif
#ifdef HEART_INVALIDATE_WAR
void heart_invalidate_war(void *,int);
int heart_need_invalidate(void);
struct buf;
void bp_heart_invalidate_war(struct buf *bp);
#endif

#endif	/* LANGUAGE_C */

#endif /* __RACER_IP30_H__ */
