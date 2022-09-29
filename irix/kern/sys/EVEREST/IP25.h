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
 * IP25.h -- IP25-specific register declarations and local resources
 */

#ifndef __SYS_EVEREST_IP25_H_
#define __SYS_EVEREST_IP25_H_

#ident "$Revision: 1.18 $"

#define _ARCSPROM

#define EV_CPU_PER_BOARD	0x04	/* Max cpus per board */

#define SCACHE_LINESIZE		128	/* 128-byte cache lines */
#define SCACHE_LINEMASK		127	/* Mask for scache lines */

#ifdef _LANGUAGE_C
#define	SBUS_TO_KVU(x)		((__psunsigned_t)(x) | K1BASE)
#else
#define	SBUS_TO_KVU(x)		((x) | K1BASE)
#endif	/* _LANGUAGE_C */

/* This macro will cause us to use the re-mapped addresses to access CC
 * chip registers, rather than using low address space addresses which
 * may overlay physical memory from 256MB to 512 MB.
 */
#define CC_ADDR(x)	(SBUS_TO_KVU((x)))

/*
 * Everest (IP25) board local resource address map.
 */
#define EV_SPNUM		CC_ADDR(0x18000008) /* Slot/Processor info */
#define EV_KZRESET		CC_ADDR(0x18000010) /* Reset the system */
#define	EV_SENDINT		CC_ADDR(0x18000100) /* Send interrupt */
#define	EV_SYSCONFIG		CC_ADDR(0x18000200) /* System configuration */
#define	EV_GRDST		CC_ADDR(0x18000300) /* Graphics write dest */
#define	EV_GRCNTL		CC_ADDR(0x18000308) /* Graphics write ctl */
#define	EV_UART_BASE		CC_ADDR(0x18000400) /* base of UART regs */
#define	EV_UART_CMD		CC_ADDR(0x18000400) /* UART command reg */
#define	EV_UART_DATA		CC_ADDR(0x18000408) /* UART data reg */
#define	EV_SCRATCH		CC_ADDR(0x18000500) /* Everest scratch reg */
#define EV_IP0			CC_ADDR(0x18000800) /* Interrupts 63 - 0 */ 
#define EV_IP1			CC_ADDR(0x18000808) /* Interrupts 127-64 */
#define	EV_HPIL			CC_ADDR(0x18000820) /* Highest pending int */
#define	EV_CEL			CC_ADDR(0x18000828) /* Current exec level */
#define EV_CIPL0		CC_ADDR(0x18000830) /* Clear pri 0 ints */
#define EV_IGRMASK		CC_ADDR(0x18000838) /* Interrupt Group mask */
#define EV_ILE			CC_ADDR(0x18000840) /* Interrupt Lvl Enable */
#define EV_CIPL124		CC_ADDR(0x18000850) /* Clear pri 1,2,4 ints */
#define EV_ERTOIP		CC_ADDR(0x18000900) /* Error/Timeout Int */
#define EV_CERTOIP		CC_ADDR(0x18000908) /* Clear ERTOIP */
#define EV_ECCSB		CC_ADDR(0x18000910) /* SB error register */
#define	EV_ERADDR		CC_ADDR(0x18000918) /* Error Address */
#define	EV_ERTAG		CC_ADDR(0x18000920) /* Error on tag RAM */
#define	EV_ERSYSBUS_LO		CC_ADDR(0x18000928) /* T5 sysbus error */
#define	EV_ERSYSBUS_HI		CC_ADDR(0x18000930)
#define	EV_DEBUG_BASE		CC_ADDR(0x18000938) /* Debug register base */
#define	EV_DEBUG0		CC_ADDR(0x18000938) /* Debug register 0 */
#define	EV_DEBUG1		CC_ADDR(0x18000940) /* Debug register 0 */
#define	EV_DEBUG2		CC_ADDR(0x18000948) /* Debug register 0 */
#define EV_RO_COMPARE		CC_ADDR(0x18000a00) /* Read only RTC comp */
#define	EV_BTRAM_BASE		CC_ADDR(0x181c0000) /* Bus tag ram base */
#define EV_BUSTAG_BASE		EV_BTRAM_BASE  		/* bus tag base */
#   define	EV_BTRAM_WAY	0x8	            /* Way of tag */
#define EV_CONFIGREG_BASE 	CC_ADDR(0x18008000) /* config register base */
#define EV_RTC			CC_ADDR(0x18020000) /* Real-time clock */
#define EV_LED_BASE		CC_ADDR(0x19004000) /* LED display base */
#define EV_SYNC_SIGNAL		CC_ADDR(0x1a000100) /* CC sync signal */

#define EV_WGINPUT_BASE		(0xbe00000018300000) /* wg/GFX input base */

/*
 * Shift and mask values for SPNUM register.
 */
#define EV_PROCNUM_MASK		0x03	/* processor number (on board) */
#define EV_SLOTNUM_MASK		0x3c	/* slot number */
#define EV_SPNUM_MASK		0x3f	/* Both slot and proc */
#define EV_PROCNUM_SHFT		0
#define EV_SLOTNUM_SHFT		2
#define EV_CCREVNUM_SHFT	6

/*
 * IP25 processor board, per-BOARD configuration registers.
 */
#define EV_A_ENABLE		0x0	/* Processor enable vector */
#define EV_A_BOARD_TYPE		0x1	/* Board type */
#define EV_A_LEVEL		0x2	/* IP25 board revision level */
#define EV_A_URGENT_TIMEOUT	0x3	/* Processor urgent timeout value */
#define EV_A_RSC_TIMEOUT	0x4	/* Chip read resource timeout */
#define EV_A_ERROR		0x6	/* Chip error register */
#define EV_A_ERROR_CLEAR	0x7	/* Clear on read error register */


/*
 * IP25 processor board, per-PROCESSOR configuration registers. Note that
 * many of the CPU local registers are shadowed here.
 */
#define EV_CFG_CMPREG0		0x10	/* LSB of the timer comparator */
#define EV_CFG_CMPREG1		0x11	/* Byte 1 of the timer comparator */
#define EV_CFG_CMPREG2		0x12	/* Byte 2 of the timer comparator */
#define EV_CFG_CMPREG3		0x13	/* MSB of the timer comparator */
#define EV_CFG_PGBRDEN		0x14	/* piggyback read enable */
#define EV_CFG_ECCHKDIS		0x15	/* ECC checking disable register */
#define	EV_CFG_SCCREV		0x19	/* SCC revision number */
#define	EV_CFG_DEBUG0_MSW	0x1a	/* Debug 0 - most significant word */
#define	EV_CFG_DEBUG0_LSW	0x1b	/* Debug 0 - least significant word */
#define	EV_CFG_DEBUG1_MSW	0x1c	/* Debug 1 - most significant word */
#define	EV_CFG_DEBUG1_LSW	0x1d	/* Debug 1 - least significant word */
#define	EV_CFG_DEBUG2_MSW	0x1e	/* Debug 2 - most significant word */
#define	EV_CFG_DEBUG2_LSW	0x1f	/* Debug 2 - least significant word */
#define	EV_CFG_IWTRIG		0x20	/* Intervention/write trigger */
#define	EV_CFG_ERTOIP		0x28	/* EV_ERTOIP shadow */
#define	EV_CFG_CERTOIP		0x29	/* EV_CERTOIP shadow */
#define	EV_CFG_ERADDR_HI	0x2a	/* EV_ERADDR shadow - MSW */
#define	EV_CFG_ERADDR_LO	0x2b	/* EV_ERADDR shadow - LSW*/
#define	EV_CFG_ERTAG		0x2c	/* EV_ERTAG shadow  */
#define	EV_CFG_ERSYSBUS_HI	0x2d	/* EV_ERSYSBUS_HI shadow */
#define	EV_CFG_ERSYSBUS_LO_HI	0x2e	/* EV_ERSYSBUS_LO shadow - LSW */
#define	EV_CFG_ERSYSBUS_LO_LO	0x2f	/* EV_ERSYSBUS_LO shadow - MSW */
#define	EV_CFG_FTOUT		0x30	/* Force time out */
#define	EV_CFG_CACHE_SZ		0x3e	/* Cache size */

/* Old names for Old crufty code */

#define EV_CMPREG0		EV_CFG_CMPREG0
#define EV_CMPREG1		EV_CFG_CMPREG1
#define EV_CMPREG2		EV_CFG_CMPREG2
#define EV_CMPREG3		EV_CFG_CMPREG3
#define EV_PGBRDEN		EV_CFG_PGBRDEN
#define EV_ECCHKDIS		EV_CFG_ECCHKDIS
#define	EV_SCCREV		EV_CFG_SCCREV
#define	EV_IWTRIG		EV_CFG_IWTRIG
#define	EV_ERADDR_HI		EV_CFG_ERADDR_HI
#define	EV_ERADDR_LO		EV_CFG_ERADDR_LO
#define	EV_ERSYSBUS_LO_LO	EV_CFG_ERSYSBUS_LO_LO
#define	EV_ERSYSBUS_LO_HI	EV_CFG_ERSYSBUS_LO_HI
#define	EV_FTOUT		EV_CFG_FTOUT
#define	EV_CACHE_SZ		EV_CFG_CACHE_SZ
/*
 * Some Usefull defines.
 */
#define	EV_CERTOIP_MASK		0x001fffff

#define	EV_GRCNTL_IC		0x4	/* ignore command */
#define	EV_GRCNTL_WS		0x2	/* Word swap  */
#define	EV_GRCNTL_ENABLE	0x1	/* enable WG */

#define	EV_ECCSB_SWM		0x8	/* single write mode */
#define	EV_ECCSB_ITP		0x4	/* ignore tag parity */
#define	EV_ECCSB_GBTP		0x2	/* generate bad tag parity */
#define	EV_ECCSB_DIAL		0x2	/* Disable internal abort (>=Rev D) */
#define	EV_ECCSB_DSBECC		0x1	/* disable single bit ECC */

/*
 * Defines for ECCCHKDIS config register. Power margining values are 
 * value for Slice 0 only.
 */
#define	EV_ECCHKDIS_15_SHFT	12
#define	EV_ECCHKDIS_15_MASK	(3<<EV_ECCHKDIS_15_SHFT)
#define	EV_ECCHKDIS_50_SHFT	10
#define	EV_ECCHKDIS_50_MASK	(3<<EV_ECCHKDIS_50_SHFT)
#define	EV_ECCHKDIS_33_SHFT	8
#define	EV_ECCHKDIS_33_MASK	(3<<EV_ECCHKDIS_33_SHFT)
#define	EV_ECCHKDIS_RU_SGP	0x4
#define	EV_ECCHKDIS_W_SGP	0x2
#define	EV_ECCHKDIS_ECC		0x1

#define	EV_MGN_NOM		0x0	/* Nominal voltage */
#define	EV_MGN_HI		0x1	/* +5% */
#define	EV_MGN_LO		0x2	/* -5% */

#define	EV_ERTAG_MASK		0x7fff


/*
 * Duplicate Cache Tag Definition: (64 bits)
 *
 *  +--------------+-------------+---------------+
 *  |   Reserved   |  Tag State  |      Tag      | 
 *  +--------------+-------------+---------------+
 *   6            2 2           2 2             0
 *   3            4 3           2 1             0
 *
 * Tag[3] - physical address bit 22 for 8MB scache and smaller
 * Tag[2] - physical address bit 21 for 4MB scache and smaller
 * Tag[1] - physical address bit 20 for 2MB scache and smaller
 * Tag[0] - physical address bit 19 for 1MB scache and smaller
 *
 */

#define	CTD_STATE_SHFT	22
#define	CTD_STATE_MASK	(0x3<<CTD_STATE_SHFT)
#   define	CTD_STATE_I	0	/* Invalid */
#   define	CTD_STATE_S	1	/* Shared */
#   define	CTD_STATE_X	3	/* Exclusive */
#define	CTD_TAG_SHFT	0
#define	CTD_TAG_MASK	(0x1fffff<<CTD_TAG_SHFT)
#define	CTD_SIZE	8		/* Size of cache tag entry */
#define	CTD_MASK	(CTD_TAG_MASK+CDT_TAG_MASK)

/*
 * IP25 processor board, cache size codes (PROCCACHSZCDE).
 * Magic formula converts "processor cache size code" to 
 * "processor cache size in bytes".  Currently, only valid
 * codes are 0,1,2,3,4.
 */
#define IP25_CACHESIZE(code) (2 << (24-(code)))

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
	sll	k1, proc, 6;				\
	dadd	k1, reg ;				\
	EV_GET_CONFIG(slot,k1,value)

#define EV_SET_PROCREG(slot,proc,reg,value)		\
	sll	k1, proc, 6;				\
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
#endif /* _LANGUAGE_C */

/*
 * Because we actually assume that the normal status register state
 * isn't zero, we define a value which is written into the status
 * register when we want to clear it.
 */

#ifdef _STANDALONE
#  define NORMAL_SR	(SR_KX|SR_FR)
#endif /* _STANDALONE */

/*
 * R10000 status register interrupt bit mask usage for IP25.
 */
#define SRB_SWTIMO	0x00000100
#define SRB_NET		0x00000200
#define SRB_DEV		0x00000400
#define SRB_TIMOCLK	0x00000800
#define SRB_UART	0x00001000
#define SRB_ERR		0x00002000
#define SRB_WGTIMO	0x00004000	/* Obsolete for IP25(SHIVA) */
#define SRB_SCHEDCLK	0x00008000

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

        /* 
	 * Mask to remove SW bits from pte. Note that the high-order
	 * address bits are overloaded with SW bits, which limits the
	 * physical addresses to 32 bits.
	 */
#define TLBLO_HWBITS            0xffffffff /* 23 bit pfn, plus CDVG */
#define TLBLO_HWBITSHIFT        0	/* A shift value, for masking */
#define TLBLO_PFNTOKDMSHFT      0	/* tlblo pfn to direct mapped */

/*
 * IP25 Led Values - of the edge of the board, the leds for each slice 
 * appear as follows, with the upper most led being the most significant
 * bit:
 *
 *	o <--- On when in everest error handler
 *	o <--- On when in cache error handler
 *	o <--\ 
 *	o <-- \
 * 	o <--- Cycle instead of flashing just bottom LED.
 *	o <--/
 */

#define	LED_EVERROR	0x20
#define	LED_CACHERR	0x10
#define	LED_CYCLE_MASK	0x0f
#define	LED_CYCLE_SHFT	4

#endif /* __SYS_EVEREST_IP25_H__ */ 

