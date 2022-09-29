/* i8259a.h - Intel 8259a PIC (Programmable Interrupt Controller) */

/* Copyright 1984-1993 Wind River Systems, Inc. */

/*
modification history
--------------------
01c,16aug93,hdn	 deleted PIC_BASE_ADRS macros.
01b,16jun93,hdn	 updated to 5.1.
01a,15may92,hdn	 written.
*/

#ifndef	__INCi8259ah
#define	__INCi8259ah

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The macro PIC_REG_ADDR_INTERVAL must be defined
 * when including this header.
 */

/* default definitions */

#define	CAST

#define PIC_ADRS(base,reg)   (CAST (base+(reg*PIC_REG_ADDR_INTERVAL)))

/* register definitions */

#define PIC_port1(base)	PIC_ADRS(base,0x00)	/* port 1. */
#define PIC_port2(base)	PIC_ADRS(base,0x01)	/* port 2. */

/* alias */

#define PIC_IMASK(base)	PIC_port2(base)		/* Interrupt mask. */
#define PIC_IACK(base)	PIC_port1(base)		/* Interrupt acknowledge. */

#ifdef __cplusplus
}
#endif

#endif	/* __INCi8259ah */
