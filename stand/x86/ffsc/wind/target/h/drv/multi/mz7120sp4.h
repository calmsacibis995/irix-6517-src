/* mz7120sp4.h - header file for Mizar mz7120 sp4 daughter board. */

/* Copyright 1984-1992 Wind River Systems, Inc. */
/*
modification history
--------------------
01f,22sep92,rrr  added support for c++
01e,26may92,rrr  the tree shuffle
01d,04oct91,rrr  passed through the ansification filter
		  -changed copyright notice
01c,05oct90,shl  added copyright notice.
                 made #endif ANSI style.
01b,29apr88,gae  added definition of INCmz7120sp4h and include m68230.h.
01a,06jan88,miz	 written.
*/

/*
This file contains I/O address and related constants
for the Mizar mz7120 sp4 daughter board.
*/

#ifndef __INCmz7120sp4h
#define __INCmz7120sp4h

#ifdef __cplusplus
extern "C" {
#endif

/* equates for m68681 serial channel - for more explanation see m68681.h, */
/*	which must be included in any file which also includes this file  */

#define	MZ_68681_BASE	((char *) 0xfe000000)

#define DUART_MRA	(MZ_68681_BASE + 0x03)	/* mode reg. A */
#define DUART_CSRA	(MZ_68681_BASE + 0x07)	/* clock select reg. A */
#define DUART_SRA	DUART_CSRA		/* status reg. A */
#define DUART_CRA	(MZ_68681_BASE + 0x0b)	/* command reg. A */
#define DUART_THRA	(MZ_68681_BASE + 0x0f)	/* transmit buffer A */
#define DUART_RHRA	DUART_THRA		/* receive buffer A */
#define DUART_ACR	(MZ_68681_BASE + 0x13)	/* auxiliary control reg. */
#define DUART_IPCR	DUART_ACR		/* input port change reg. */
#define DUART_IMR	(MZ_68681_BASE + 0x17)	/* int. mask reg. */
#define DUART_ISR	DUART_IMR		/* int. status reg. */
#define DUART_CTUR	(MZ_68681_BASE + 0x1b)	/* counter timer upper reg. */
#define DUART_CTLR	(MZ_68681_BASE + 0x1f)	/* counter timer lower reg. */
#define DUART_MRB	(MZ_68681_BASE + 0x23)	/* mode reg. B */
#define DUART_CSRB	(MZ_68681_BASE + 0x27)	/* clock select reg. B */
#define DUART_SRB	DUART_CSRB		/* status reg. B */
#define DUART_CRB	(MZ_68681_BASE + 0x2b)	/* command reg. B */
#define DUART_THRB	(MZ_68681_BASE + 0x2f)	/* transmit buffer B */
#define DUART_RHRB	DUART_THRB		/* receive buffer B */
#define DUART_IVR	(MZ_68681_BASE + 0x33)	/* int. vector reg. */
#define DUART_OPCR	(MZ_68681_BASE + 0x37)	/* output port config. reg. */
#define DUART_IP	DUART_OPCR		/* input port */
#define DUART_SOPBC	(MZ_68681_BASE + 0x3b)	/* set output port bits */
#define DUART_CTRON	DUART_SOPBC		/* counter on */
#define DUART_ROPBC	(MZ_68681_BASE + 0x3f)	/* reset output port bits */
#define DUART_CTROFF	DUART_ROPBC		/* counter off */

/* end of equates for m68681 */


/* equates for m68230 pi/t channel - for more explanation see m68230.h,  */
/*	which must be included in any file which also includes this file */

#define	MZ_68230_BASE	((char *) 0xfe000080)

#define PIT_PGCR	(MZ_68230_BASE + 0x83)	/* port general control reg. */
#define PIT_PSRR	(MZ_68230_BASE + 0x87)	/* port service request reg. */
#define PIT_PADDR	(MZ_68230_BASE + 0x8b)	/* port A data direction reg. */
#define PIT_PBDDR	(MZ_68230_BASE + 0x8f)	/* port B data direction reg. */
#define PIT_PCDDR	(MZ_68230_BASE + 0x93)	/* port C data direction reg. */
#define PIT_PIVR	(MZ_68230_BASE + 0x97)	/* port int. vector reg. */
#define PIT_PACR	(MZ_68230_BASE + 0x9b)	/* port A control reg. */
#define PIT_PBCR	(MZ_68230_BASE + 0x9f)	/* port B control reg. */
#define PIT_PADR	(MZ_68230_BASE + 0xa3)	/* port A data reg. */
#define PIT_PBDR	(MZ_68230_BASE + 0xa7)	/* port B data reg. */
#define PIT_PAAR	(MZ_68230_BASE + 0xab)	/* port A alternate reg. */
#define PIT_PBAR	(MZ_68230_BASE + 0xaf)	/* port B alternate reg. */
#define PIT_PCDR	(MZ_68230_BASE + 0xb3)	/* port C data reg. */
#define PIT_PSR		(MZ_68230_BASE + 0xb7)	/* port status reg. */
#define PIT_TCR		(MZ_68230_BASE + 0xc3)	/* timer control reg. */
#define PIT_TIVR	(MZ_68230_BASE + 0xc7)	/* timer int. vector reg. */
#define PIT_CPRHB	(MZ_68230_BASE + 0xcf)	/* ctr. preload reg. - MSB */
#define PIT_CPRMB	(MZ_68230_BASE + 0xd3)	/* ctr. preload reg. - MB */
#define PIT_CPRLB	(MZ_68230_BASE + 0xd7)	/* ctr. preload reg. - LSB */
#define PIT_CRHB	(MZ_68230_BASE + 0xdf)	/* counter reg. - high byte */
#define PIT_CRMB	(MZ_68230_BASE + 0xe3)	/* counter reg. - middle byte */
#define PIT_CRLB	(MZ_68230_BASE + 0xe7)	/* counter reg. - low byte */
#define PIT_TSR		(MZ_68230_BASE + 0xeb)	/* timer status reg. */

/* end of equates for m68230 */

#ifdef __cplusplus
}
#endif

#endif /* __INCmz7120sp4h */
