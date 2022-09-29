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

/*
 * IP21.h -- IP21-specific register declarations and local resources
 */

#ifndef __SYS_IP21_H__
#define __SYS_IP21_H__

#ident "$Revision: 1.23 $"

#define _ARCSPROM

#define EV_CPU_PER_BOARD	0x02	/* Max cpus per board */

#define GCACHE_LINESIZE		128	/* 128-byte cache lines */
#define GCACHE_LINEMASK		127	/* Mask for gcache lines */

#ifdef _LANGUAGE_C
#define	SBUS_TO_KVU(x)		((__psunsigned_t)(x) | K1BASE)
#else
#define	SBUS_TO_KVU(x)		((x) | K1BASE)
#endif	/* _LANGUAGE_C */

/* This macro will cause us to use the re-mapped addresses to access CC
 * chip registers, rather than using low address space addresses which
 * may overlay physical memory from 256MB to 512 MB.
 */
#define CC_ADDR(x)	(SBUS_TO_KVU((x) | 0x0000000400000000))

/*
 * IP21 board local resource address map - New/different (in TFP)
 */
#define BB_STARTUP_CTRL	CC_ADDR(0x18000018)	/* Gcache startup control */
#define BB_BUSTAG_ADDR	CC_ADDR(0x18080000)	/* bus tag address */
#define BB_BUSTAG_ST	CC_ADDR(0x180c0000)	/* bus tag state */
#define BB_PTAG_E_ADDR	CC_ADDR(0x18100000)	/* proc tag even address */
#define BB_PTAG_E_ST	CC_ADDR(0x18140000)	/* proc tag even state */
#define BB_PTAG_O_ADDR	CC_ADDR(0x18180000)	/* proc tag odd address */
#define BB_PTAG_O_ST	CC_ADDR(0x181c0000)	/* proc tag odd state */
#define BB_SET_ALLOW	CC_ADDR(0x18400000)	/* Gcache set allow */

/* The following defines are for setting allowable gcache sets in BB_SET_ALLOW
 */
#define BB_GC_SET_0	0x1
#define BB_GC_SET_1	0x2
#define BB_GC_SET_2	0x4
#define BB_GC_SET_3	0x8
#define BB_GC_SET_NORM	0xf

/* The following addresses were defined to let kernel compile.
 * Their existence needs to be confirmed with HW folks.
 */
/*
 * Everest board local resource address map
 */
#define EV_SPNUM	CC_ADDR(0x18000008)	/* Slot / Processor info */
#define EV_KZRESET	CC_ADDR(0x18000010)	/* Reset the system */
#define	EV_SENDINT	CC_ADDR(0x18000100)	/* Send interrupt */
#define	EV_SYSCONFIG	CC_ADDR(0x18000200)	/* System configuration */
#define	EV_WGDST	CC_ADDR(0x18000300)	/* Write Gatherer dest */
#define	EV_WGA		CC_ADDR(0x18000310)	/* Write Gatherer A flush */
#define	EV_WGB		CC_ADDR(0x18000320)	/* Write Gatherer B flush */
#define	EV_UART_BASE	CC_ADDR(0x18000400)	/* base of UART regs */
#define	EV_UART_CMD	CC_ADDR(0x18000400)	/* UART command reg */
#define	EV_UART_DATA	CC_ADDR(0x18000408)	/* UART data reg */
#define EV_IP0		CC_ADDR(0x18000800)	/* Interrupts 63 - 0 */ 
#define EV_IP1		CC_ADDR(0x18000808)	/* Interrupts 127-64 */
#define	EV_HPIL		CC_ADDR(0x18000820)	/* Highest pending int level */
#define	EV_CEL		CC_ADDR(0x18000828)	/* Current execution level */
#define EV_CIPL0	CC_ADDR(0x18000830)	/* Clear pri 0 interrupts */
#define EV_CIPL124	CC_ADDR(0x18000850)	/* Clear pri 1,2,4 ints */
#define EV_IGRMASK	CC_ADDR(0x18000838)	/* Interrupt Group mask */
#define EV_ILE		CC_ADDR(0x18000840)	/* Interrupt Level Enable */
#define EV_ERTOIP	CC_ADDR(0x18000900)	/* Error/Timeout Int Pending */
#define EV_CERTOIP	CC_ADDR(0x18000908)	/* Clear ERTOIP register */
#define EV_DIAGREG	CC_ADDR(0x18000910)	/* Diagnostic register */
#define EV_RO_COMPARE	CC_ADDR(0x18000a00)	/* Read only RTC compare */
#define EV_CONFIGREG_BASE CC_ADDR(0x18008000)	/* config register base */
#define EV_RTC		CC_ADDR(0x18020000)	/* Real-time clock */
#define EV_VOLTAGE_CTRL	CC_ADDR(0x19002000)	/* Voltage margin ctrl - av.
						 * on rev 7 and later boards.
						 */
#define EV_LED_BASE	CC_ADDR(0x19004000)	/* LED display base */

/*
 * Usefull define specifying all bits to clear in ERTOIP register.
 */
#define	EV_CERTOIP_MASK	0xffff

/*
 * Newer revisions of the IP21 board no longer have an EAROM (which is not
 * used by software due to hardware bugs). Writing to the EAROM addresses is
 * ignored. Reading EAROM addresses return the following data:
 *
 *	Bits<7:4> contain the rev of the board
 *	Bits<3:0> Dchip Error bits
 *		<3> Dchip 3   Data Receive Error
 *		<2> Dchip 2   Data Receive Error
 *		<1> Dchip 1   Data Receive Error
 *		<0> Dchip 0   Data Receive Error
 */
#define EV_D_ERROR	CC_ADDR(0x19000000)	/* D chip error bits */
#define EV_IP21_REV	CC_ADDR(0x19000000)	/* Board revision number */

#define EV_IP21REV_MASK	0xf0		/* Bitmask for board rev */
#define EV_IP21REV_SHFT	4		/* Shift for board rev */
#define EV_DERR_MASK	0x0f		/* Bitmask for D chip err */
#define	EV_IP21LOWREV	0x60		/* 1st board with a revision reg. */
#define	EV_IP21REV_VCR	0x70		/* 1st board with a Voltage Ctrl Reg. */

#define	EV_VCR_LOW3V	0x1		/* Set 3v power supply to -5% */
#define	EV_VCR_HIGH3V	0x2		/* Set 3v power supply to +5% */
#define	EV_VCR_LOW5V	0x4		/* Set 5v power supply to -5% */
#define	EV_VCR_HIGH5V	0x8		/* Set 5v power supply to +5% */

/*
 * These values used to be in the EAROM, but the EAROM interface is broken,
 * so we use constants instead.
 */
#define EV_RTCFREQ	47619048	/* 47.619048 MHz (21 ns clock cycle) */
#define EV_RSCTOUT	5000000		/* RSC timeout: 0.1 second */
#define EV_IO4ETOUT	4000000		/* IO4 Ebus timeout: 0.8 second */
#define EV_AURGTOUT	128		/* A chip urgent timeout */

#define	EV_CONFIG1_BASE		CC_ADDR(0x18008800)	/* config base slot 1 */
#define	EV_CONFIG2_BASE		CC_ADDR(0x18009000)	/* etc */
#define	EV_CONFIG3_BASE		CC_ADDR(0x18009800)
#define	EV_CONFIG4_BASE 	CC_ADDR(0x1800a000)
#define	EV_CONFIG5_BASE 	CC_ADDR(0x1800a800)
#define	EV_CONFIG6_BASE 	CC_ADDR(0x1800b000)
#define	EV_CONFIG7_BASE 	CC_ADDR(0x1800b800)
#define	EV_CONFIG8_BASE 	CC_ADDR(0x1800c000)
#define	EV_CONFIG9_BASE 	CC_ADDR(0x1800c800)
#define	EV_CONFIGa_BASE 	CC_ADDR(0x1800d000)
#define	EV_CONFIGb_BASE 	CC_ADDR(0x1800d800)
#define	EV_CONFIGc_BASE 	CC_ADDR(0x1800e000)
#define	EV_CONFIGd_BASE 	CC_ADDR(0x1800e800)
#define	EV_CONFIGe_BASE 	CC_ADDR(0x1800f000)
#define	EV_CONFIGf_BASE 	CC_ADDR(0x1800f800)

#define	EV_SYNC_SIGNAL		CC_ADDR(0x1a000100)

#define EV_WGINPUT_BASE         (0x8000000018300000) /* write gather input */
#define EV_WGSTATUS             CC_ADDR(0x18300000) /* write gather stat */

#define EV_SYNC_SIGNAL		CC_ADDR(0x1a000100) /* CC sync signal */

/*
 * Shift and mask values for SPNUM register.
 */
#define EV_PROCNUM_MASK		0x02	/* processor number (on board) */
#define EV_SLOTNUM_MASK		0x3c	/* slot number */
#define EV_SPNUM_MASK		0x3e	/* Both slot and proc */
#define EV_PROCNUM_SHFT		1
#define EV_SLOTNUM_SHFT		2
#define EV_CCREVNUM_SHFT	6


/*
 * IP21 processor board, per-BOARD configuration registers.
 *
 * NOTE: Must confirm these with HW folks.  For now, we need them just
 * to compile the PROM and the kernel.
 */
#define EV_A_ENABLE		0x0	/* Processor enable vector */
#define EV_A_BOARD_TYPE		0x1	/* Board type */
#define EV_A_LEVEL		0x2	/* IP21 board revision level */
#define EV_A_URGENT_TIMEOUT	0x3	/* Processor urgent timeout value */
#define EV_A_RSC_TIMEOUT	0x4	/* Chip read resource timeout */
#define EV_A_ERROR		0x6	/* Chip error register */
#define EV_A_ERROR_CLEAR	0x7	/* Clear on read error register */


/*
 * IP21 processor board, per-PROCESSOR configuration registers.
 *
 * NOTE: Must confirm these with HW folks.  For now, I need them just
 * to compile a kernel.
 */
#define EV_CMPREG0		0x10	/* LSB of the timer comparator */
#define EV_CMPREG1		0x11	/* Byte 1 of the timer comparator */
#define EV_CMPREG2		0x12	/* Byte 2 of the timer comparator */
#define EV_CMPREG3		0x13	/* MSB of the timer comparator */
#define EV_PGBRDEN		0x14	/* piggyback read enable */
#define EV_ECCHKDIS		0x15	/* ECC checking disable register */
#define EV_CACHE_SZ		0x3e	/* processor cache size code */


/*
 * TFP status register interrupt bit mask usage for IP21.
 *
 * NOTE: Verify with HW folks.  I have defined them the same as IP19
 * so that the kernel will compile.
 */
#define SRB_SWTIMO	0x00000100
#define SRB_NET		0x00000200
#define SRB_DEV		0x00000400
#define SRB_TIMOCLK	0x00000800
#define SRB_UART	0x00001000
#define SRB_ERR		0x00002000
#define SRB_WGTIMO	0x00004000
#define	SRB_GPARITYE	0x00010000	/* G-cache parity error - even bank */
#define	SRB_GPARITYO	0x00020000	/* G-cache parity error - odd bank */
#define SRB_SCHEDCLK	0x00040000	/* It's at IP10 on Blackbird CPU */

#define II_SPL1		SRB_NET|SRB_DEV|SRB_UART|SRB_ERR|SRB_WGTIMO|SRB_SCHEDCLK
#define II_SPL3		SRB_DEV|SRB_UART|SRB_ERR|SRB_WGTIMO|SRB_SCHEDCLK
#define II_SPLNET	SRB_DEV|SRB_UART|SRB_ERR|SRB_WGTIMO|SRB_SCHEDCLK
#define II_SPLTTY	SRB_DEV|SRB_ERR|SRB_WGTIMO|SRB_SCHEDCLK
#define II_SPLHI	SRB_DEV|SRB_ERR|SRB_WGTIMO
#define II_SPLTLB	SRB_DEV|SRB_ERR|SRB_WGTIMO
#define II_SPLPROF	SRB_DEV|SRB_ERR|SRB_WGTIMO
#define II_SPL7		0

#define II_CEL1		EVINTR_LEVEL_BASE
#define II_CELNET	EVINTR_LEVEL_BASE
#define II_CEL3		(EVINTR_LEVEL_MAXLODEV+1)
#define II_CELTTY	(EVINTR_LEVEL_MAXHIDEV+1)
#define II_CELHI	(EVINTR_LEVEL_HIGH+1)
#define II_CELTLB	(EVINTR_LEVEL_TLB+1)
#define II_CELPROF	(EVINTR_LEVEL_EPC_PROFTIM+1)
#define II_CEL7		EVINTR_LEVEL_MAX

#if LANGUAGE_ASSEMBLY

/*
 * EV_GET_SPNUM reads the slot and processor number of 
 * the calling processor.  The slot and proc parameters 
 * must both be registers.  The proc parameter may be 
 * register zero. 
 */
#define EV_GET_SPNUM(slot,proc) 			\
	dli	slot, EV_SPNUM ; 			\
	ld	slot, 0(slot) ; 			\
	nop 	;					\
	andi	slot, EV_SPNUM_MASK;			\
	and	proc, slot, EV_PROCNUM_MASK ;		\
	srl	proc, EV_PROCNUM_SHFT ;			\
	srl	slot, EV_SLOTNUM_SHFT
/*
 * The next two macros are used to read and write config
 * registers.  For both macros, slot must be a register,
 * and reg can be either a register or an immediate value.
 * For EV_GET_CONFIG, value must be a register; in
 * EV_SET_CONFIG, value can be either a register or an
 * immediate value.  These macros trash the contents of
 * registers k0 and k1.
 */
#define EV_GET_CONFIG(slot,reg,value)			\
	sll	k0, slot, 11 ;				\
	dadd	k0, EV_CONFIGREG_BASE ;			\
	dadd	value, zero, reg ; 			\
	sll	value, 3 ;				\
	dadd	k0, value ;				\
	ld	value, 0(k0) ;				\
	nop

#define EV_SET_CONFIG(slot,reg,value)			\
	sll	k0, slot, 11 ;				\
	dadd	k0, EV_CONFIGREG_BASE ;			\
	dadd	k1, zero, reg ;				\
	sll	k1, 3 ;					\
	dadd	k0, k1 ;				\
	or	k1, zero, value ;			\
	sd	k1, 0(k0) ;				\
	nop

/*
 * Macros for setting processor config registers.  Note that
 * the slot and proc parameters must both be registers.  The
 * reg and value parameters can be either an immediate or 
 * a register.  These two macros trash the contents of k0 and
 * k1.
 */
#define EV_GET_PROCREG(slot,proc,reg,value)		\
	sll	k1, proc, 7 ;				\
	dadd	k1, reg ;				\
	EV_GET_CONFIG(slot,k1,value)

#define EV_SET_PROCREG(slot,proc,reg,value)		\
	sll	k1, proc, 7;				\
	dadd	k1, reg ;				\
	EV_SET_CONFIG(slot,k1,value)

#endif /* LANGUAGE_ASSEMBLY */

/*
 * The following is temporary until the arrival of 64-bit
 * support in the C compiler.
 */ 

#if _LANGUAGE_C

#define EV_GET_CONFIG(slot,reg) \
	load_double_lo((long long *)EV_CONFIGADDR((slot),0,(reg)))

#define EV_GET_CONFIG_HI(slot,reg) \
	load_double_hi((long long *)EV_CONFIGADDR((slot),0,(reg)))

#define EV_SET_CONFIG(slot,reg,value) \
	store_double_lo((long long *)EV_CONFIGADDR((slot),0,(reg)), (value))

#define EV_GET_CONFIG_NOWAR(slot,reg) \
	load_double_lo_nowar((long long *)EV_CONFIGADDR((slot),0,(reg)))

#define EV_GET_CONFIG_HI_NOWAR(slot,reg) \
	load_double_hi_nowar((long long *)EV_CONFIGADDR((slot),0,(reg)))

#endif /* _LANGUAGE_C */

/*
 * Because we actually assume that the normal status register state
 * isn't zero, we define a value which is written into the status
 * register when we want to clear it.
 */

#ifdef _STANDALONE
#  define NORMAL_SR	(SR_FR | SR_IE)
#endif /* _STANDALONE */


#ifndef TFP_PTE64
/* The ptes are packed into 32 bits.  Need to shift left in order to get
 * correct format for TLB entryLo register.
 */
#define	TLBLO_HWBITSHIFT	3
#endif

#endif /* __SYS_IP21_H__ */
