/* i8253.h - Intel 8253 PIT (Programmable Interrupt Timer) */

/* Copyright 1984-1993 Wind River Systems, Inc. */

/*
modification history
--------------------
01b,16jun93,hdn	 updated to 5.1.
01a,20may92,hdn	 written.
*/

#ifndef	__INCi8253h
#define	__INCi8253h

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The macro PIT_REG_ADDR_INTERVAL must be defined
 * when including this header.
 */

/* default definitions */

#define	CAST

#define PIT_ADRS(base,reg)   (CAST (base+(reg*PIT_REG_ADDR_INTERVAL)))

/* register definitions */

#define PIT_CNT0(base)	PIT_ADRS(base,0x00)	/* Counter 0. */
#define PIT_CNT1(base)	PIT_ADRS(base,0x01)	/* Counter 1. */
#define PIT_CNT2(base)	PIT_ADRS(base,0x02)	/* Counter 2. */
#define PIT_CMD(base)	PIT_ADRS(base,0x03)	/* Control word. */

#ifdef __cplusplus
}
#endif

#endif	/* __INCi8253h */
