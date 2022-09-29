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
 * IP19.h -- IP19-specific register declarations and local resources
 */

#ifndef __SYS_EVEREST_IP19_H__
#define __SYS_EVEREST_IP19_H__

#ident "$Revision: 1.30 $"

#define _ARCSPROM

#define EV_CPU_PER_BOARD	0x04	/* Max cpus per board */

#ifdef _LANGUAGE_C
#define	SBUS_TO_KVU(x)		((__psunsigned_t)(x) | K1BASE)
#else
#define	SBUS_TO_KVU(x)		((x) | K1BASE)
#endif	/* _LANGUAGE_C */


/*
 * IP19 board local resource address map
 */
#define EV_SPNUM		SBUS_TO_KVU(0x18000008)	/* Slot/Processor info*/
#define EV_KZRESET		SBUS_TO_KVU(0x18000010)	/* Reset the system */
#define	EV_SENDINT		SBUS_TO_KVU(0x18000100)	/* Send interrupt */
#define	EV_SYSCONFIG		SBUS_TO_KVU(0x18000200)	/* System config */
#define	EV_WGDST		SBUS_TO_KVU(0x18000300)	/* Write Gather dest */
#define	EV_WGCNTRL		SBUS_TO_KVU(0x18000308)	/* Write Gather ctrl */
#define	EV_WGCLEAR		SBUS_TO_KVU(0x18000340)	/* Write Gather clear */
#define	EV_UART_BASE		SBUS_TO_KVU(0x18000400)	/* base of UART regs */
#define	EV_UART_CMD		SBUS_TO_KVU(0x18000400)	/* UART command reg */
#define	EV_UART_DATA		SBUS_TO_KVU(0x18000408)	/* UART data reg */
#define EV_IP0			SBUS_TO_KVU(0x18000800)	/* Interrupts 63 - 0 */ 
#define EV_IP1			SBUS_TO_KVU(0x18000808)	/* Interrupts 127-64 */
#define	EV_HPIL			SBUS_TO_KVU(0x18000820)	/* Highest pending int lvl */
#define	EV_CEL			SBUS_TO_KVU(0x18000828)	/* Current exc. level */
#define EV_CIPL0		SBUS_TO_KVU(0x18000830) /* Clear pri 0 intrs */
#define EV_CIPL124		SBUS_TO_KVU(0x18000850)	/* Clr pri 1,2,4 ints */	
#define EV_IGRMASK		SBUS_TO_KVU(0x18000838)	/* Intr Group mask */
#define EV_ILE			SBUS_TO_KVU(0x18000840)	/* Intr Level Enable */
#define EV_ERTOIP		SBUS_TO_KVU(0x18000900)	/* Error/Timeout Int Pending */
#define EV_CERTOIP		SBUS_TO_KVU(0x18000908)	/* Clear ERTOIP reg */
#define EV_ECCSB_DIS		SBUS_TO_KVU(0x18000910)	/* ECC Sngl-bit err disable */
#define EV_RO_COMPARE		SBUS_TO_KVU(0x18000a00)	/* Read only RTC cmp */
#define EV_CONFIGREG_BASE	SBUS_TO_KVU(0x18008000)	/* config reg base */
#define EV_RTC			SBUS_TO_KVU(0x18020000)	/* Real-time clock */
#define EV_BUSTAG_BASE		SBUS_TO_KVU(0x181c0000)	/* bus tag base */
#define EV_WGINPUT_BASE		SBUS_TO_KVU(0x18300000)	/* write gather input */
#define EV_WGCOUNT		SBUS_TO_KVU(0x18300000)	/* write gather count */
#define EV_EAROM_BASE		SBUS_TO_KVU(0x19000000)	/* config EAROM */
#define EV_LED_BASE		SBUS_TO_KVU(0x19004000)	/* LED display base */
#define EV_SYNC_SIGNAL		SBUS_TO_KVU(0x1a000100) /* CC sync signal */
#define EV_PROM_BASE		SBUS_TO_KVU(0x1c000000)	/* boot PROM */

/*
 * Shift and mask values for SPNUM register.
 */
#define EV_PROCNUM_MASK		0x03	/* processor number (on board) */
#define EV_SLOTNUM_MASK		0x3c	/* slot number */
#define EV_SPNUM_MASK		0x3f	/* Both slot and proc */
#define EV_PROCNUM_SHFT		0
#define EV_SLOTNUM_SHFT		2

/*
 * The following values are stored in the EAROM
 */
#define EV_EBUSRATE0_LOC	SBUS_TO_KVU(0x19000100)	/* EBUS freq (Hz) LSB */
#define EV_EBUSRATE1_LOC	SBUS_TO_KVU(0x19000108)	/* EBUS freq byte 1 */
#define EV_EBUSRATE2_LOC	SBUS_TO_KVU(0x19000110)	/* EBUS freq byte 2 */
#define EV_EBUSRATE3_LOC	SBUS_TO_KVU(0x19000118)	/* EBUS freq MSB */
#define EV_PGBRDEN_LOC		SBUS_TO_KVU(0x19000120)	/* Piggyback Rd Enbl */
#define EV_CACHE_SZ_LOC		SBUS_TO_KVU(0x19000128)	/* Size of s'ary cache*/
#define EV_IW_TRIG_LOC		SBUS_TO_KVU(0x19000130)	/* IW_TRIG value */
#define EV_RR_TRIG_LOC		SBUS_TO_KVU(0x19000138)	/* RR_TRIG value */
#define EV_EPROCRATE0_LOC	SBUS_TO_KVU(0x19000140)	/* CPU freq (Hz) LSB */
#define EV_EPROCRATE1_LOC	SBUS_TO_KVU(0x19000148)	/* CPU freq byte 1 */
#define EV_EPROCRATE2_LOC	SBUS_TO_KVU(0x19000150)	/* CPU freq byte 2 */
#define EV_EPROCRATE3_LOC	SBUS_TO_KVU(0x19000158)	/* CPU freq MSB */
#define EV_RTCFREQ0_LOC		SBUS_TO_KVU(0x19000160)	/* RTC freq (Hz) LSB */ 
#define EV_RTCFREQ1_LOC		SBUS_TO_KVU(0x19000168)	/* RTC freq byte 2 */
#define EV_RTCFREQ2_LOC		SBUS_TO_KVU(0x19000170)	/* RTC freq byte 3 */
#define EV_RTCFREQ3_LOC		SBUS_TO_KVU(0x19000178)	/* RTC freq MSB */
#define EV_WCOUNT0_LOC		SBUS_TO_KVU(0x19000180)	/* EAROM Write cnt LSB*/
#define EV_WCOUNT1_LOC		SBUS_TO_KVU(0x19000188)	/* EAROM Write cnt MSB*/
#define EV_ECCENB_LOC		SBUS_TO_KVU(0x19000190)	/* CC chip ECC enab */
#define EV_CKSUM0_LOC		SBUS_TO_KVU(0x19000200)	/* EAROM cksum LSB */
#define EV_CKSUM1_LOC		SBUS_TO_KVU(0x19000208)	/* EAROM cksum MSB */
#define EV_NCKSUM0_LOC		SBUS_TO_KVU(0x19000210)	/* EAROM ~cksum LSB */
#define EV_NCKSUM1_LOC		SBUS_TO_KVU(0x19000218)	/* EAROM ~cksum MSB */

#define EV_BE_LOC		SBUS_TO_KVU(0x19000000)	/* Endian Byte */

#define EAROM_BE_MASK		(1 << 2)	/* Bit of BE in EAROM */
#define EAROM_SC_MASK		(1 << 4)	/* 2ndary cache enable bit */

#define EV_CHKSUM_LEN		48		/* Checksum 48 bytes of
						 * EAROM.  Cannot be changed. */
#define EV_EAROM_BYTE0		0x0c		/* Byte zero of the EAROM is
						 * compared against this.
						 * Cannot be changed.
						 * Big endian bit is set.
						 */

#define	EV_CONFIG1_BASE		SBUS_TO_KVU(0x18008800)	/* config base slot 1 */
#define	EV_CONFIG2_BASE		SBUS_TO_KVU(0x18009000)	/* etc */
#define	EV_CONFIG3_BASE		SBUS_TO_KVU(0x18009800)
#define	EV_CONFIG4_BASE 	SBUS_TO_KVU(0x1800a000)
#define	EV_CONFIG5_BASE 	SBUS_TO_KVU(0x1800a800)
#define	EV_CONFIG6_BASE 	SBUS_TO_KVU(0x1800b000)
#define	EV_CONFIG7_BASE 	SBUS_TO_KVU(0x1800b800)
#define	EV_CONFIG8_BASE 	SBUS_TO_KVU(0x1800c000)
#define	EV_CONFIG9_BASE 	SBUS_TO_KVU(0x1800c800)
#define	EV_CONFIGa_BASE 	SBUS_TO_KVU(0x1800d000)
#define	EV_CONFIGb_BASE 	SBUS_TO_KVU(0x1800d800)
#define	EV_CONFIGc_BASE 	SBUS_TO_KVU(0x1800e000)
#define	EV_CONFIGd_BASE 	SBUS_TO_KVU(0x1800e800)
#define	EV_CONFIGe_BASE 	SBUS_TO_KVU(0x1800f000)
#define	EV_CONFIGf_BASE 	SBUS_TO_KVU(0x1800f800)

/*
 * IP19 processor board, per-BOARD configuration registers.
 */
#define EV_A_ENABLE		0x0	/* Processor enable vector */
#define EV_A_BOARD_TYPE		0x1	/* Board type */
#define EV_A_LEVEL		0x2	/* IP19 board revision level */
#define EV_A_URGENT_TIMEOUT	0x3	/* Processor urgent timeout value */
#define EV_A_RSC_TIMEOUT	0x4	/* Chip read resource timeout */
#define EV_A_ERROR		0x6	/* Chip error register */
#define EV_A_ERROR_CLEAR	0x7	/* Clear on read error register */

/*
 * IP19 processor board, per-PROCESSOR configuration registers.
 */
#define EV_CMPREG0		0x10	/* LSB of the timer comparator */
#define EV_CMPREG1		0x11	/* Byte 1 of the timer comparator */
#define EV_CMPREG2		0x12	/* Byte 2 of the timer comparator */
#define EV_CMPREG3		0x13	/* MSB of the timer comparator */
#define EV_PGBRDEN		0x14	/* piggyback read enable */
#define EV_ECCHKDIS		0x15	/* ECC checking disable register */
#define EV_CPERIOD		0x16	/* clock period */
#define EV_PROC_DATARATE	0x17	/* processor data rate */
#define EV_WGRETRY_TOUT		0x19	/* write gatherer retry time out
					 * on CC1 only */
#define EV_CCREV_REG		0x19	/* CC rev (on CC2 and up) */
#define EV_IW_TRIG		0x20	/* IW trigger */
#define EV_RR_TRIG		0x21	/* RR trigger */
#define EV_CACHE_SZ		0x3e	/* processor cache size code */

/*
 * Values of EV_CCREV_REG.  For CC rev 1, this register can be written
 * so the value is only guaranteed at reset.
 */
#define EV_CCREV_1		0x1f	/* Value in EV_CCREV_REG for CC rev 1 */
#define EV_CCREV_2		0x1e	/* Value in EV_CCREV_REG for CC rev 2 */

#define	EV_CERTOIP_MASK		0x3ff

/*
 * IP19 processor board, cache size codes (PROCCACHSZCDE).
 * Magic formula converts "processor cache size code" to 
 * "processor cache size in bytes".  Currently, only valid
 * codes are 0,1,2,3,4.
 */
#define IP19_CACHESIZE(code) (2 << (22-(code)))

#if LANGUAGE_ASSEMBLY

/*
 * EV_GET_SPNUM reads the slot and processor number of 
 * the calling processor.  The slot and proc parameters 
 * must both be registers.  The proc parameter may be 
 * register register zero. 
 */
#define EV_GET_SPNUM(slot,proc) 			\
	LI	slot, EV_SPNUM ; 			\
	ld	slot, 0(slot) ; 			\
	nop 	;					\
	and	proc, slot, EV_PROCNUM_MASK ;		\
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
	add	k0, EV_CONFIGREG_BASE ;			\
	add	value, zero, reg ; 			\
	sll	value, 3 ;				\
	add	k0, value ;				\
	ld	value, 0(k0) ;				\
	nop

#define EV_SET_CONFIG(slot,reg,value)			\
	sll	k0, slot, 11 ;				\
	add	k0, EV_CONFIGREG_BASE ;			\
	add	k1, zero, reg ;				\
	sll	k1, 3 ;					\
	add	k0, k1 ;				\
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
	sll	k1, proc, 6 ;				\
	add	k1, reg ;				\
	EV_GET_CONFIG(slot,k1,value)

#define EV_SET_PROCREG(slot,proc,reg,value)		\
	sll	k1, proc, 6;				\
	add	k1, reg ;				\
	EV_SET_CONFIG(slot,k1,value)

#endif /* LANGUAGE_ASSEMBLY */

/*
 * The PROM can't use 32-bit long long support, so instead
 * we call funky assembly language stubs which do doubleword
 * operations for us.  Maybe this will the arrival of 64-bit
 * native support in the C compiler.
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
 * R4000 status register interrupt bit mask usage for IP19.
 */
#define SRB_SWTIMO	0x00000100
#define SRB_NET		0x00000200
#define SRB_DEV		0x00000400
#define SRB_TIMOCLK	0x00000800
#define SRB_UART	0x00001000
#define SRB_ERR		0x00002000
#define SRB_WGTIMO	0x00004000
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
#define	II_CELTLB	(EVINTR_LEVEL_TLB+1)
#define II_CELPROF	(EVINTR_LEVEL_EPC_PROFTIM+1)
#define II_CEL7		EVINTR_LEVEL_MAX


        /* Mask to remove SW bits from pte. Note that the high-order
	 * address bits are overloaded with SW bits, which limits the
	 * physical addresses to 32 bits.
	 */
#define TLBLO_HWBITS            0xfffffff8      /* 23 bit pfn, plus CDVG */
#define TLBLO_HWBITSHIFT        3               /* A shift value, for masking */
#define TLBLO_PFNTOKDMSHFT      3               /* tlblo pfn to direct mapped */
#if !defined(ECCF_CACHE_ERR)
/* KLUDGE FOR NOW */

/*
 * IP19 ECC error handler defines.
 * Define an additional exception frame for the ECC handler.
 * Save 3 more registers on this frame: C0_CACHE_ERR, C0_TAGLO,
 * and C0_ECC.
 * Call this an ECCF_FRAME.
 */
#define ECCF_CACHE_ERR	0
#define ECCF_TAGLO	1
#define ECCF_ECC	2
#define ECCF_ERROREPC	3
#define ECCF_PADDR	4
#define	ECCF_PADDRHI	5
#define ECCF_SIZE	(6 * sizeof(long))


#endif  /* ECCF_CACHE_ERR */



#endif /* __SYS_EVEREST_IP19_H__ */ 
