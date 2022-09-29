/* s68155.h - Signetics 68155 Interrupt Handler header file */

/* Copyright 1984-1992 Wind River Systems, Inc. */
/*
modification history
--------------------
01e,22sep92,rrr  added support for c++
01d,26may92,rrr  the tree shuffle
01c,04oct91,rrr  passed through the ansification filter
		  -changed copyright notice
01b,05oct90,shl  added copyright notice.
                 made #endif ANSI style.
01a,05Dec88,jcf   written.
*/

#ifndef __INCs68155h
#define __INCs68155h

#ifdef __cplusplus
extern "C" {
#endif


/* IRHDL addresses */

#define IHDL_PTR(base)		((char *) ((base) + 0x01)) /* pointer -> CRn */
#define IHDL_CR(base)		((char *) ((base) + 0x03)) /* control reg */
#define IHDL_VEC(base)		((char *) ((base) + 0x05)) /* interrupt vec. */
#define IHDL_LOCAL_MASK(base)	((char *) ((base) + 0x07)) /* local int. mask */
#define IHDL_LOCAL_STATUS(base)	((char *) ((base) + 0x09)) /* local status */
#define IHDL_BUS_MASK(base)	((char *) ((base) + 0x0b)) /* system bus mask */
#define IHDL_BUS_STATUS(base)	((char *) ((base) + 0x0d)) /* system bus stat */
#define IHDL_LAST_ACK(base)	((char *) ((base) + 0x0f)) /* last acknowledge*/

/* control registers */

#define	IHDL_CR1		0x1	/* local interrupt level 1 */
#define	IHDL_CR2		0x2	/* local interrupt level 2 */
#define	IHDL_CR3		0x3	/* local interrupt level 3 */
#define	IHDL_CR4		0x4	/* local interrupt level 4 */
#define	IHDL_CR5		0x5	/* local interrupt level 5 */
#define	IHDL_CR6		0x6	/* local interrupt level 6 */

/* equates for control register */

#define IHDL_CR_HIGH		0x1	/* interrupt asserted high (unlikely) */
#define IHDL_CR_EDGE		0x2	/* input is edge sensitive (likely) */
#define IHDL_CR_VECTOR		0x4	/* IRHDL provides vector */

/* equates for vector register */

#define IHDL_VEC_MASK		0xf8	/* low three bits always read as zero */

/* equates for local mask */

#define IHDL_LOCAL_MASK_NMI_VEC	0x1	/* 1 = active high */
#define IHDL_LOCAL_MASK_1	0x2	/* 1 = local interrupt 1 enabled */
#define IHDL_LOCAL_MASK_2	0x4	/* 1 = local interrupt 2 enabled */
#define IHDL_LOCAL_MASK_3	0x8	/* 1 = local interrupt 3 enabled */
#define IHDL_LOCAL_MASK_4	0x10	/* 1 = local interrupt 4 enabled */
#define IHDL_LOCAL_MASK_5	0x20	/* 1 = local interrupt 5 enabled */
#define IHDL_LOCAL_MASK_6	0x40	/* 1 = local interrupt 6 enabled */
#define IHDL_LOCAL_MASK_NMI	0x80	/* 1 = NMI enabled */

/* equates for local status */

#define IHDL_LOCAL_STATUS_1	0x2	/* 1 = local interrupt 1 */
#define IHDL_LOCAL_STATUS_2	0x4	/* 1 = local interrupt 2 */
#define IHDL_LOCAL_STATUS_3	0x8	/* 1 = local interrupt 3 */
#define IHDL_LOCAL_STATUS_4	0x10	/* 1 = local interrupt 4 */
#define IHDL_LOCAL_STATUS_5	0x20	/* 1 = local interrupt 5 */
#define IHDL_LOCAL_STATUS_6	0x40	/* 1 = local interrupt 6 */
#define IHDL_LOCAL_STATUS_NMI	0x80	/* 1 = NMI */

/* equates for bus mask */
					/* low bit always reads as zero */
#define IHDL_BUS_MASK_1		0x2	/* 1 = bus interrupt 1 enabled */
#define IHDL_BUS_MASK_2		0x4	/* 1 = bus interrupt 2 enabled */
#define IHDL_BUS_MASK_3		0x8	/* 1 = bus interrupt 3 enabled */
#define IHDL_BUS_MASK_4		0x10	/* 1 = bus interrupt 4 enabled */
#define IHDL_BUS_MASK_5		0x20	/* 1 = bus interrupt 5 enabled */
#define IHDL_BUS_MASK_6		0x40	/* 1 = bus interrupt 6 enabled */
#define IHDL_BUS_MASK_7		0x80	/* 1 = bus interrupt 7 enabled */

/* equates for bus status */
					/* low bit always reads as zero */
#define IHDL_BUS_STATUS_1	0x2	/* 1 = bus interrupt 1 */
#define IHDL_BUS_STATUS_2	0x4	/* 1 = bus interrupt 2 */
#define IHDL_BUS_STATUS_3	0x8	/* 1 = bus interrupt 3 */
#define IHDL_BUS_STATUS_4	0x10	/* 1 = bus interrupt 4 */
#define IHDL_BUS_STATUS_5	0x20	/* 1 = bus interrupt 5 */
#define IHDL_BUS_STATUS_6	0x40	/* 1 = bus interrupt 6 */
#define IHDL_BUS_STATUS_7	0x80	/* 1 = bus interrupt 7 */

/* equates for last acknowledge */

#define IHDL_LAST_ACK_BUS_1	0x1
#define IHDL_LAST_ACK_BUS_2	0x2
#define IHDL_LAST_ACK_BUS_3	0x3
#define IHDL_LAST_ACK_BUS_4	0x4
#define IHDL_LAST_ACK_BUS_5	0x5
#define IHDL_LAST_ACK_BUS_6	0x6
#define IHDL_LAST_ACK_BUS_7	0x7
#define IHDL_LAST_ACK_LOCAL_1	0x9
#define IHDL_LAST_ACK_LOCAL_2	0xa
#define IHDL_LAST_ACK_LOCAL_3	0xb
#define IHDL_LAST_ACK_LOCAL_4	0xc
#define IHDL_LAST_ACK_LOCAL_5	0xd
#define IHDL_LAST_ACK_LOCAL_6	0xe
#define IHDL_LAST_ACK_NMI	0xf

#ifdef __cplusplus
}
#endif

#endif /* __INCs68155h */
