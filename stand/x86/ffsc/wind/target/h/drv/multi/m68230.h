/*  m68230.h - Motorola m68230 parallel/timer chip */

/* Copyright 1984-1992 Wind River Systems, Inc. */
/*
modification history
--------------------
01i,22sep92,rrr  added support for c++
01h,26may92,rrr  the tree shuffle
01g,04oct91,rrr  passed through the ansification filter
		  -changed ASMLANGUAGE to _ASMLANGUAGE
		  -changed copyright notice
01f,05oct90,shl  added copyright notice.
                 made #endif ANSI style.
01e,20mar90,shl  fixed ZERO_DET_CTL typo introduced in 01c.
01d,10oct89,rld  Allow for ASM & register offset.
01c,27jul88,gae+mcl  fixed ZERO_DET_CTL typo.
01b,25mar88,dnw  added register definitions, cleaned up.
01a,08jan88,miz	 written.
*/

/*
This file contains constants for the Motorola m68230 parallel/timer chip.
*/

#ifndef __INCm68230h
#define __INCm68230h

#ifdef __cplusplus
extern "C" {
#endif

#ifdef  _ASMLANGUAGE
#define CAST
#else
#define CAST (char *)
#endif	/* _ASMLANGUAGE */

#define PIT_ADRS(base, reg)    (CAST base + (reg * PIT_REG_ADDR_INTERVAL))

/* register definitions */

#define PIT_PGCR(base)  PIT_ADRS (base, 0x00)   /* port genl cntrl reg */
#define PIT_PSRR(base)  PIT_ADRS (base, 0x01)   /* port service req reg */
#define PIT_PADDR(base) PIT_ADRS (base, 0x02)   /* port A data dir reg */
#define PIT_PBDDR(base) PIT_ADRS (base, 0x03)   /* port B data dir reg */
#define PIT_PCDDR(base) PIT_ADRS (base, 0x04)   /* port C data dir reg */
#define PIT_PIVR(base)  PIT_ADRS (base, 0x05)   /* port int vector reg */
#define PIT_PACR(base)  PIT_ADRS (base, 0x06)   /* port A control reg */
#define PIT_PBCR(base)  PIT_ADRS (base, 0x07)   /* port B control reg */
#define PIT_PADR(base)  PIT_ADRS (base, 0x08)   /* port A data reg */
#define PIT_PBDR(base)  PIT_ADRS (base, 0x09)   /* port B data reg */
#define PIT_PAAR(base)  PIT_ADRS (base, 0x0a)   /* port A alternate reg */
#define PIT_PBAR(base)  PIT_ADRS (base, 0x0b)   /* port B alternate reg */
#define PIT_PCDR(base)  PIT_ADRS (base, 0x0c)   /* port C data reg */
#define PIT_PSR(base)   PIT_ADRS (base, 0x0d)   /* port status reg */
#define PIT_TCR(base)   PIT_ADRS (base, 0x10)   /* timer control reg */
#define PIT_TVIR(base)  PIT_ADRS (base, 0x11)   /* timer int vec reg */
#define PIT_CPRH(base)  PIT_ADRS (base, 0x13)   /* ctr preload reg high */
#define PIT_CPRM(base)  PIT_ADRS (base, 0x14)   /* ctr preload reg med */
#define PIT_CPRL(base)  PIT_ADRS (base, 0x15)   /* ctr preload reg low */
#define PIT_CNTRH(base) PIT_ADRS (base, 0x17)   /* count reg high */
#define PIT_CNTRM(base) PIT_ADRS (base, 0x18)   /* count reg medium */
#define PIT_CNTRL(base) PIT_ADRS (base, 0x19)   /* count reg low */
#define PIT_TSR(base)   PIT_ADRS (base, 0x1a)   /* timer status reg */

/* port general control register */

#define	PORT_MODE_3	0xc0		/* bidirectional 16-bit mode */
#define	PORT_MODE_2	0x80		/* bidirectional 8-bit mode */
#define	PORT_MODE_1	0x40		/* unidirectional 16-bit mode */
#define	PORT_MODE_0	0x00		/* unidirectional 8-bit mode */
#define	H34_ENABLE	0x20		/* 0 = diable, 1 = enable */
#define	H12_ENABLE	0x10		/* 0 = diable, 1 = enable */
#define	H4_SENSE	0x08		/* 0 = active low, 1 = active high */
#define	H3_SENSE	0x04		/* 0 = active low, 1 = active high */
#define	H2_SENSE	0x02		/* 0 = active low, 1 = active high */
#define	H1_SENSE	0x01		/* 0 = active low, 1 = active high */

/* port service request register */

#define	DMA_REQ_1	0x60		/* dma request 1 */
#define	DMA_REQ_0	0x40		/* dma request 0 */
#define	DMA_NONE	0x20		/* no dma */
#define	PIACK		0x10		/* 0 = PC6, 1 = PIACK */
#define	PIRQ		0x08		/* 0 = PC5, 1 = PIRQ */
#define	PIPC_7		0x07		/* H4S > H3S > H2S > H1S */
#define	PIPC_6		0x06		/* H4S > H3S > H1S > H2S */
#define	PIPC_5		0x05		/* H3S > H4S > H2S > H1S */
#define	PIPC_4		0x04		/* H3S > H4S > H1S > H2S */
#define	PIPC_3		0x03		/* H2S > H1S > H4S > H3S */
#define	PIPC_2		0x02		/* H1S > H2S > H4S > H3S */
#define	PIPC_1		0x01		/* H2S > H1S > H3S > H4S */
#define	PIPC_0		0x00		/* H1S > H2S > H3S > H4S */

/* port interrupt vector register */

#define	PORT_IVEC_MASK	0xfc		/* port interrupt vector mask */

/* port A/B control register */

#define	PORT_SUBMODE_3	0xc0		/* port submode 3 */
#define	PORT_SUBMODE_2	0x80		/* port submode 2 */
#define	PORT_SUBMODE_1	0x40		/* port submode 1 */
#define	PORT_SUBMODE_0	0x00		/* port submode 0 */
#define	H24_CONTROL_7	0x38		/* control field = 7 */
#define	H24_CONTROL_6	0x30		/* control field = 6 */
#define	H24_CONTROL_5	0x28		/* control field = 5 */
#define	H24_CONTROL_4	0x20		/* control field = 4 */
#define	H24_CONTROL_3	0x18		/* control field = 3 */
#define	H24_CONTROL_2	0x10		/* control field = 2 */
#define	H24_CONTROL_1	0x08		/* control field = 1 */
#define	H24_CONTROL_0	0x00		/* control field = 0 */
#define	H24_INT_ENABLE	0x04		/* 0 = disabled, 1 = enabled */
#define	H13_SVCRQ_ENABLE 0x02		/* 0 = disabled, 1 = enabled */
#define	H13_STATUS	0x01		/* status */

/* port status register */

#define	H4_LEVEL	0x80		/* H4 level */
#define	H3_LEVEL	0x40		/* H3 level */
#define	H2_LEVEL	0x20		/* H2 level */
#define	H1_LEVEL	0x10		/* H1 level */
#define	H4S		0x08		/* H4 status */
#define	H3S		0x04		/* H3 status */
#define	H2S		0x02		/* H2 status */
#define	H1S		0x01		/* H1 status */

/* timer control register */

#define	TIMER_CTL_7	0xe0		/* timer control 7 */
#define	TIMER_CTL_6	0xc0		/* timer control 6 */
#define	TIMER_CTL_5	0xa0		/* timer control 5 */
#define	TIMER_CTL_4	0x80		/* timer control 4 */
#define	TIMER_CTL_3	0x60		/* timer control 3 */
#define	TIMER_CTL_2	0x40		/* timer control 2 */
#define	TIMER_CTL_1	0x20		/* timer control 1 */
#define	TIMER_CTL_0	0x00		/* timer control 0 */
#define	ZERO_DET_CTL	0x10		/* zero detect control */
					/* 10 = rollover, 00 = use preload */
#define	CLOCK_CTL_3	0x06		/* clock control 3 */
#define	CLOCK_CTL_2	0x04		/* clock control 2 */
#define	CLOCK_CTL_1	0x02		/* clock control 1 */
#define	CLOCK_CTL_0	0x00		/* clock control 0 */
#define	TIMER_ENABLE	0x01		/* 0 = disabled, 1 = enabled */

/* timer status register */

#define	ZERO_DET_STATUS	0x01		/* read:  zero detect status bit */
#define PIT_ACK_INTR    0x01            /* write: acknowledge interrupt */


#ifdef __cplusplus
}
#endif

#endif /* __INCm68230h */
