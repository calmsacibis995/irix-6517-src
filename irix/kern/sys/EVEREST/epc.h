/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1993, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * epc.h -- EPC register location definitions.
 */

#ifndef __SYS_EPC_H__
#define __SYS_EPC_H__

#ident "$Revision: 1.47 $"

#include "sys/reg.h"
#include "sys/EVEREST/everest.h"
#include "sys/EVEREST/io4.h"
#include "sys/EVEREST/evconfig.h"

#define EPC_LW(_r)	((_r) | 0x00a00000)

#ifdef _LANGUAGE_C

#include "sys/types.h"

#define EPC_GET(_r, _i, _reg) \
	(evreg_t)load_double((long long *)(SWIN_BASE((_r), (_i)) + (_reg)))
#define EPC_SET(_r, _i, _reg, _value) \
	store_double((long long *)(SWIN_BASE((_r),(_i)) + (_reg)), (long long)(_value))

#define	EPC_SETBASE(r,i) \
	(ei->ei_epcbase = (caddr_t)(SWIN_BASE((r),(i)))) /* 8 byte aligned */
#ifdef	_MIPSEB
#define EPC_SETW(r, v)	(*(volatile u_int*)(ei->ei_epcbase + (r) + 4) = (v))
#define EPC_GETW(r)	*(volatile u_int*)(ei->ei_epcbase + (r) + 4)
#define EPC_SETB(r,v)	(*(volatile u_char*)(ei->ei_epcbase + (r) + 7) = (v))
#endif

#endif /* _LANGUAGE_C */


#define EPC_REGION	1	/* by definition, set up by prom */
#define EPC_ADAPTER	(EVCFGINFO->ecfg_epcioa)

/*
 * PBUS devices -- little window address ranges
 */
#define	EPC_PROM	0x0000	/* pdev 0,1	Boot Prom */
#define	EPC_ENETPROM	0x2000	/* pdev 2	Ethernet ID PROM */
#define	EPC_NVRTC	0x3000	/* pdev 3	Clock/Timer/NVRAM */
#define	EPC_DUART0	0x4000	/* pdev 4	console kbd, mouse */
#define	EPC_DUART1	0x5000	/* pdev 5	2 rs232 */
#define	EPC_DUART2	0x6000	/* pdev 6	1 rs232 + 1 rs422 */
#define	EPC_PPORT	0x7000	/* pdev 7	Parallel Port Control Reg */
#define	EPC_EXTINT	0x9000	/* pdev 9	External Interrupt Ctrl Reg */
#define EPC_PPSTAT	0x9000	/* pdev 9	Parallel Port Status Reg */
#define PBUS_BASE(addr) SWIN_BASE(EPC_REGION, EPC_ADAPTER) + (addr)

/*
 * Some pbus devices need to use byte-sized addressing.  The devices generally
 * don't much care about a0,a1,a2, but the EPC needs to drive them properly
 * so the byte ends up in the proper lane.
 */
#ifdef _MIPSEB
#  define BYTE_SELECT 7
#else
#  define BYTE_SELECT 0
#endif /* _MIPSEB */

/*
 * PBUS devices -- big window address ranges.  Currently, only
 * the Flash EPROMs need to be addressed through large windows.
 */
#define EPC_LWIN_LOPROM		0x000000
#define EPC_LWIN_HIPROM		0x100000


/*
 * EPC Interrupt control registers 
 */
#define EPC_ISTAT	0x0000a100	/* Interrupt status/mask register */
#define EPC_IMSET	0x0000a108	/* Interrupt mask set register */
#define EPC_IMRST	0x0000a110	/* Interrupt mask reset register */

/* EPC_ISTAT,_IMSET,_IMRST bits */
#define	EPC_INTR_DUART0		0x01
#define	EPC_INTR_DUART12	0x02	/* DUART 1,2 intrs wired together */
#define	EPC_INTR_ENET		0x04	/* intr from EDLC via external logic */
#define	EPC_INTR_PROFTIM	0x08
#define	EPC_INTR_SPARE		0x10
#define	EPC_INTR_PPORT		0x20
#define	EPC_INTR_UNUSED		0x40	/* (Original EPC Enet intr) */
#define	EPC_INTR_ERROR		0x80

#define EPC_IIDDUART0	0x0000a118	/* DUART0 interrupt no. and dest. */
#define EPC_IIDDUART1	0x0000a120	/* DUART1 interrupt no. and dest. */
#define EPC_IIDENET	0x0000a128	/* EPC/EDLC Interrupt no. and dest. */
#define EPC_IIDPROFTIM	0x0000a130	/* PROFTIM interrupt no. and dest. */
/*
 * 2 SPARE interrupt pins are ORed to one interrupt source.  
 * This will be used for cluster support.
 */
#define EPC_IIDSPARE	0x0000a138	/* SPARE interrupt no. and dest. */
#define EPC_IIDPPORT	0x0000a140	/* par port interrupt no. and dest. */
#define EPC_IIDINTR4	0x0000a138	/* external interrupts no. and dest. */
#define EPC_IIDERROR	0x0000a150	/* ERROR intr. no. and dest. */

/*
 * EPC SEEQ/EDLC ethernet control registers
 */
#define	EPC_ENET_BASE	0x0000a200
#define	EPC_EADDR_BASE	EPC_ENET_BASE
#define EPC_EADDR0	EPC_ENET_BASE	/* Ethernet address byte 0 */
#define EPC_EADDR1	0x0000a208	/* Ethernet address byte 1 */
#define EPC_EADDR2	0x0000a210	/* Ethernet address byte 2 */
#define EPC_EADDR3	0x0000a218	/* Ethernet address byte 3 */
#define EPC_EADDR4      0x0000a220      /* Ethernet address byte 4 */
#define EPC_EADDR5      0x0000a228      /* Ethernet address byte 5 */
#define EPC_TCMD	0x0000a230	/* Transmit command register */
#define EPC_RCMD	0x0000a238	/* Receive command register */

#define EPC_TBASELO	0x0000a250	/* Low bits of xmit buff base */
#define EPC_TBASEHI	0x0000a258	/* High bits of xmit buff base */
#define EPC_TLIMIT	0x0000a260	/* Xmit Buffer limit */
#define EPC_TINDEX	0x0000a268	/* Xmit Buffer index */
#define EPC_TTOP	0x0000a270	/* Xmit Buffer top */
#define EPC_TBPTR	0x0000a278	/* Xmit Buffer byte pointer */
#define EPC_TSTAT	0x0000a280	/* Transmit status */
#define EPC_TITIMER	0x0000a288	/* Transmit interrupt timer */

#define EPC_RBASELO	0x0000a2a0	/* Rcv Buffer base addr */
#define EPC_RBASEHI	0x0000a2a8	/* High bits of rcv buffer base addr */
#define EPC_RLIMIT	0x0000a2b0	/* Rcv Buffer limit */
#define EPC_RINDEX 	0x0000a2b8	/* Rcv Buffer index */
#define EPC_RTOP	0x0000a2c0	/* Rcv Buffer top */
#define EPC_RBPTR	0x0000a2c8	/* Rcv Buffer byte pointer */
#define EPC_RSTAT	0x0000a2d0	/* Rcv status */
#define EPC_RITIMER	0x0000a2d8	/* Rcv interrupt timer */

/* EPC_TLIMIT/EPC_RLIMIT bits */
#define	EPC_2BUFS	0
#define	EPC_4BUFS	1
#define	EPC_8BUFS	2
#define	EPC_16BUFS	3
#define	EPC_32BUFS	4
#define	EPC_64BUFS	5
#define	EPC_128BUFS	6
#define	EPC_256BUFS	7

#define	EPC_LIMTON(l)	(lmap[l])
/*
 * The above macro expects the following array:
 * int lmap[8] = { 2, 4, 8, 16, 32, 64, 128, 256 };
 */

/* take advantage of binary nature of nbufs value */
#define	EPC_BUF_INCR(bn, mask)	((bn) + 1 & (mask))
#define	EPC_BUF_DECR(bn, mask)	((bn) - 1 & (mask))
#define	EPC_BUF_SUB(bn, v, mask) 	((unsigned)(bn) - (v) & mask)
#define	EPC_RBN_INCR(bn)	EPC_BUF_INCR(bn, ei->ei_rbufs - 1)
#define	EPC_RBN_DECR(bn)	EPC_BUF_DECR(bn, ei->ei_rbufs - 1)
#define	EPC_TBN_INCR(bn)	EPC_BUF_INCR(bn, ei->ei_tbufs - 1)
#define	EPC_TBN_DECR(bn)	EPC_BUF_DECR(bn, ei->ei_tbufs - 1)

#define	EPC_ENET_DATA	2		/* 1st byte of packet */
#define	EPC_ENET_STAT	2044		/* 1st byte of count/status */
#define	EPC_ENET_LENMASK 0x7ff		/* length is low 11 bits of stat word */
#define	EPC_ENET_STSHFT	16
#define	EPC_ENET_STMASK	0xff
#define	EPC_ENET_BUFSZ	2048

/* map buffer index to buffer pointer */
#define	EPC_ENET_TBP(tbn)	(ei->ei_tbase + tbn * EPC_ENET_BUFSZ)
#define	EPC_ENET_RBP(rbn)	(ei->ei_rbase + rbn * EPC_ENET_BUFSZ)


/*
 * LXT901 Universal Ethernet Interface Adapter regs -- pbus "device 8"
 */
#define	EPC_LXT_STAT	0x00008000	/* some LXT901 pins + SQE ctrl (r/w) */
#define	EPC_LXT_LOOP	0x00008010	/* loopback enable -- 1 bit (w) */
#define	EPC_EDLC_SELF	0x00008018	/* EDLC self rcv enable -- 1 bit (w) */
#define	EPC_TOT_COL	0x00008008	/* total collisions -- 16 bits (r) */
#define	EPC_TOT_COL_MASK    0xffff	
#define	EPC_EARLY_COL	0x00008010	/* early collisions -- 16 bits (r) */
#define	EPC_EARLY_COL_MASK  0xffff	
#define	EPC_LATE_COL	0x00008000	/* late collisions -- 8 bits (r) */
#define	EPC_LATE_COL_MASK     0xff

/*
 * EPC Parallel port hardware parameters.
 */

#define EPC_PPBLKSZ	4096		/* EPC plp maximum buffer size */
#define EPC_PPMAXXFER	4095		/* Can transfer (block size) - 1 */ 
#define EPC_PPBLKALGN	4096		/* Buffer must be 4096-byte aligned */

/*
 * EPC Parallel port control registers 
 */
#define EPC_PPBASELO	0x0000a300	/* Lo bits of Parallel Port Base Addr */
#define EPC_PPBASEHI	0x0000a308	/* Hi bits of Parallel Port Base Addr */
#define EPC_PPLEN	0x0000a310	/* PPort xmit/recv DMA byte length */
#define EPC_PPCOUNT	0x0000a318	/* PPort xmit/recv DMA byte count */
#define EPC_PPCTRL	0x0000a320	/* PPort control register */

/*
 * EPC Parallel Port Control Register Bits
 */
#define EPC_PPSTB_L	0		/* Strobe active low */
#define EPC_PPSTB_H	1		/* Strobe active high */
#define EPC_PPBUSY_L	0		/* Busy active low */
#define EPC_PPBUSY_H	(1 << 1)	/* Busy active high */
#define EPC_PPACK_L	0		/* Ack active low */
#define EPC_PPACK_H	(1 << 2)	/* Ack active high */
#define EPC_PPSACKMODE	(1 << 3)	/* "Soft Acknowledge" mode */
#define EPC_PPBUSYMODE	(1 << 4)	/* "Busy mode" */
#define EPC_PPSTARTDMA	(1 << 5)	/* Start DMA */
#define EPC_PPSTOPDMA	0		/* Stop DMA */
#define EPC_PPIN	(1 << 6)	/* PP Input mode */
#define EPC_PPOUT	0		/* PP Output mode */
#define EPC_PPPLSSHIFT	7		/* Strobe pulse width (us) shift */
#define EPC_PPHLDSHIFT	16		/* Strobe-data hold time (us) shift */
#define EPC_PPSTPSHIFT	24		/* Strobe-data setup time (us) shift */
#define EPC_STBMASK	0xffffff80	/* Mask for the EPC strobe params */
/*
 * EPC Parallel Port pull-{up,down} defines (in EPC reset register)
 */
#define EPC_PPSTRBPUL_L	0
#define EPC_PPSTRBPUL_H	(1 << 5)
#define EPC_PPBUSYPUL_L	0
#define EPC_PPBUSYPUL_H (1 << 6)
#define EPC_PPPULLMASK	(EPC_PPSTRBPUL_H | EPC_PPBUSYPUL_H)

/* 
 * EPC Parallel port PBUS control/status bits in "EPC_PPORT" register.
 */

/* Status: */
#define EPC_PPFAULT	1
#define EPC_PPNOINK	2
#define EPC_PPNOPAPER	4
#define EPC_PPONLINE	8

/* Control: */
#define EPC_PPPRT	1	/* Straight */
#define EPC_PPRESET	2	/* Inverted */

/*
 * EPC Miscellaneous registers 
 */
#define EPC_PRST	0x0000a400	/* P dev reset status */
#define EPC_PRSTSET	0x0000a408	/* P dev reset set */
#define EPC_PRSTCLR	0x0000a410	/* P dev reset clear */
#define EPC_IERR	0x0000a418	/* IBUS error register */
#define EPC_IERRC	0x0000a420	/* IBUS error clear */
#define EPC_EINFO1	0x0000a428	/* Additional error information */
#define EPC_EINFO2	0x0000a430	/* Second additional error info reg */

/*
 * EPC PRST, PRSTSET, PRSTCLR bits
 */
#define	EPC_ENETHOLD_ARM	0x010	/* Arm enet intr. hold-off timer */
/*
 * Bits 7,8 are used to program the enet intr. hold-off timer duration.
 * The hold-off circuit is programmed to the appropriate duration by using
 * EPC_PRSTSET/CLR to set/clr each to result in the below value for each
 * duration.
 */
#ifdef	_LANGUAGE_C
#define	EPC_SETENETHOLD(v)	\
{ \
	if (v) { \
		EPC_SETW(EPC_PRSTSET, v); \
	} \
	if (~v & EPC_ENETHOLD_MASK) { \
		EPC_SETW(EPC_PRSTCLR, ~v & EPC_ENETHOLD_MASK); \
	} \
}

#endif	/* _LANGUAGE_C */

#define	EPC_ENETHOLD_MASK	0x180
#define	EPC_ENETHOLD_0_0	0x180	/* Immediate interrupt */
#define	EPC_ENETHOLD_0_8	0x100	/* .8 ms hold-off */
#define	EPC_ENETHOLD_1_5	0x080	/* 1.5 ms hold-off */
#define	EPC_ENETHOLD_2_5	0x000	/* 2.5 ms hold-off */

#define	EPC_PRST_EDLC		0x200	/* Reset EDLC */

#define	EPC_PDEVRSTALL		0x02f

/* 
 * Dallas DS1397 RAMified Real-time clock control and data registers
 *
 * To read/write a register in the RTC address map first write the number
 * of the register to the "indirect address register" and then read/write
 * the value from/to the "data register" -- each register must be accessed
 * as a byte.
 *
 * 	a9==1	selects RTC register bank		0x200
 *	a3==0	selects indirect address register	0x008
 *	a3==1	selects data register			0x000
 *
 * Finally, lines a0,a1,a2 must be set for the EPC's sake to direct the
 * byte to the proper lane.  This is obviously endian sensitive.
*/
#define NVR_RTC		0x200		/* Real Time Clock register bank */
#define	NVR_RTC_ADDR	0x000		/* RTC indirect address register */
#define	NVR_RTC_DATA	0x008		/* RTC data register */
#define NVR_XRAM	0x00		/* Extended RAM register bank */
#define NVR_XRAMPAGE	(NVR_XRAM|(0x20 << 3))	/* Extended Ram selection reg */

#define NVR_SEC		0x00 /* RTC Seconds register */
#define NVR_SECALRM	0x01 /* RTC Seconds alarm reg */
#define NVR_MIN		0x02 /* RTC Minutes register */
#define NVR_MINALRM	0x03 /* RTC Minutes alarm reg */
#define NVR_HOUR	0x04 /* RTC Hours register */
#define NVR_HOURALRM	0x05 /* RTC Hours alarm register */
#define NVR_WEEKDAY	0x06 /* RTC Day or week (1 = Sun) */
#define NVR_DAY		0x07 /* RTC Date of month (0-31) */
#define NVR_MONTH	0x08 /* RTC Month (1-12) */
#define NVR_YEAR	0x09 /* RTC year (0-99) */
#define NVR_RTCA	0x0a /* RTC Register A */
#define	NVR_INTR_SLOW	0x00 /* RTCA Periodic Interrupt 0 KHz */
#define	NVR_INTR_FAST	0x06 /* RTCA Periodic Interrupt 1.024 KHz */
#define NVR_INTR_SLOW_RATE 0
#define NVR_INTR_FAST_RATE 1024
#define	NVR_OSC_ON	0x20 /* RTCA DV1=1 turns on oscillator */
#define NVR_DV_MASK	0x70 /* RTCA DV2/1/0 set, for use as mask */
#define NVR_RTCB	0x0b /* RTC Register B */
#define	NVR_SET		0x80 /* RTCB SET=1 == while programming RTC regs */
#define	NVR_PIE		0x40 /* RTCB Periodic Interrupt Enable */
#define	NVR_DM		0x04 /* RTCB DM=1 == binary data format */
#define	NVR_2412	0x02 /* RTCB 24/12=1 == 24-hour mode */
#define NVR_RTCC	0x0c /* RTC Register C */
#define NVR_RTCD	0x0d /* RTC Register D */
#define NVR_VRT		0x80 /* RTCD Valid Ram and Time */

#define XRAM_PAGEMASK	0xfe0
#define XRAM_PAGESHFT	5
#define XRAM_BYTEMASK	0x1f

#ifdef _LANGUAGE_C
#  define XRAM_PAGE(_x) ( ((_x) & XRAM_PAGEMASK) >> XRAM_PAGESHFT )
#  define XRAM_REG(_x)	( ((_x) & XRAM_BYTEMASK) << 3)

#define EPC_RTC_READ(r)	\
(\
*(volatile char *)(PBUS_BASE(EPC_NVRTC) + NVR_RTC + NVR_RTC_ADDR + BYTE_SELECT) = (r),\
*(volatile u_char *)(PBUS_BASE(EPC_NVRTC) + NVR_RTC + NVR_RTC_DATA + BYTE_SELECT) \
)

#define EPC_RTC_WRITE(r,v) \
(\
*(volatile char *)(PBUS_BASE(EPC_NVRTC) + NVR_RTC + NVR_RTC_ADDR + BYTE_SELECT) = (r), \
*(volatile u_char *)(PBUS_BASE(EPC_NVRTC) + NVR_RTC + NVR_RTC_DATA + BYTE_SELECT) = (v) \
)

#endif /* _LANGUAGE_C */

/* Register A bits */
#define NVRA_UIP	0x80		/* Update-in-progress bit */
#define NVRA_STARTOSC	0x20		/* Start oscillator value */

/* Register B bits */
#define NVRB_SET	0x80		/* Switch off periodic updates */
#define NVRB_PIE	0x40		/* Periodic Interrupt Enable */
#define NVRB_AIE	0x20		/* Alaram Interrupt Enable */
#define NVRB_UIE	0x10		/* Update encoded Interrupt enable */
#define NVRB_SQWE	0x80		/* Square Wave Enable */
#define NVRB_DM		0x40		/* Date mode (1 = binary, 0 = BCD) */
#define NVRB_MILTIME	0x20		/* Hours format (1 = 24-hour mode) */
#define NVRB_DSE	0x10		/* Daylight Savings Enable */

/* Register C bits */	
#define NVRC_IRQF	0x80		/* Interrupt Requested Flag */
#define NVRC_PF		0x40		/* Periodic Interrupt Flag */
#define NVRC_AF		0x20		/* Alarm Interrupt Flag */
#define NVRC_UF		0x10		/* Update ended interrupt Flag */
 
/* Register D bits */
#define NVRD_VRT	0x80		/* Valid RAM and time bit */


/*
 * Full address of DUART Registers
 */
#define DUART0	PBUS_BASE(EPC_DUART0)
#define DUART1	PBUS_BASE(EPC_DUART1)
#define DUART2	PBUS_BASE(EPC_DUART2)

#define CHNA_DATA_OFFSET	0x18
#define CHNB_DATA_OFFSET        0x10
#define CHNA_CNTRL_OFFSET       0x8
#define CHNB_CNTRL_OFFSET	0x0

/*
 * We need to grab byte BYTE_SELECT (7 for EB, 0 for EL) of the PBUS registers
 * to get the actual data from the UART.  Needless to say, we also need
 * to use byte-sized loads and stores to access these. 
 */

#define DUART0A_DATA    (DUART0 + CHNA_DATA_OFFSET + BYTE_SELECT)
#define DUART0B_DATA    (DUART0 + CHNB_DATA_OFFSET + BYTE_SELECT)
#define DUART1A_DATA    (DUART1 + CHNA_DATA_OFFSET + BYTE_SELECT)
#define DUART1B_DATA    (DUART1 + CHNB_DATA_OFFSET + BYTE_SELECT)
#define DUART2A_DATA    (DUART2 + CHNA_DATA_OFFSET + BYTE_SELECT)
#define DUART2B_DATA    (DUART2 + CHNB_DATA_OFFSET + BYTE_SELECT)

#define DUART0A_CNTRL   (DUART0 + CHNA_CNTRL_OFFSET + BYTE_SELECT)
#define DUART0B_CNTRL   (DUART0 + CHNB_CNTRL_OFFSET + BYTE_SELECT)
#define DUART1A_CNTRL   (DUART1 + CHNA_CNTRL_OFFSET + BYTE_SELECT)
#define DUART1B_CNTRL   (DUART1 + CHNB_CNTRL_OFFSET + BYTE_SELECT)
#define DUART2A_CNTRL   (DUART2 + CHNA_CNTRL_OFFSET + BYTE_SELECT)
#define DUART2B_CNTRL   (DUART2 + CHNB_CNTRL_OFFSET + BYTE_SELECT)

#ifdef _LANGUAGE_C
/* Define the structure where epc_init saves information about the 
 * epc devices found during boot time
 */


typedef struct epc {
	ioadap_t	ioadap;
} epc_t;

#define	epc_slot(x)	epcs[x].ioadap.slot
#define	epc_adap(x)	epcs[x].ioadap.padap
#define	epc_swin(x)	epcs[x].ioadap.swin

extern epc_t	epcs[];
extern int 	numepcs;

/* Prototype definitions */
extern caddr_t epc_probe (int);
extern void epc_intr_proftim(eframe_t *, void *);
extern void epc_intr_spare(eframe_t *, void *);
extern void epc_intr_pport(eframe_t *, void *);
extern void epc_intr_error(eframe_t *, void *);
extern void epc_init(int, int, int);
extern void  epc_intr_init(__psunsigned_t, int);

#endif /* _LANGUAGE_C */

#endif /* __SYS_EPC_H__ */
