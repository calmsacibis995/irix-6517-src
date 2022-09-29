/**************************************************************************
 *									  *
 * 		 Copyright (C) 1993, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "sys/vde_hw.h: $Revision: 1.1 $"

/*
 * This file contains declarations for data structures and routines used
 * only within the vde driver.  For the most part,  they describe the
 * VMAX hardware.
 */

/*
 * struct vde_desc
 * This structure represents a DMA descriptor in main memory.
 * Since it's shared between the vde hardware and the
 * cpu, it needs to be volatile.
 */
typedef struct vde_desc
{
    volatile unsigned long   vme_base;	/* Base VME address */
    union {
	volatile unsigned long word;	/* To access as a word. */
	struct {
	    volatile unsigned	iack	: 1;	/* Iack cycle? */
	    volatile unsigned	a_mod	: 6;	/* Address modifier */
	    volatile unsigned	d_size	: 2;	/* 00=>D8, 01=>D16 11=>D32 10=>D32(RSVD) */
	    volatile unsigned	direc	: 1;	/* 1==VME->GIO 0==GIO->VME */
	    volatile unsigned	ror	: 1;	/* Release on Request */
	    volatile unsigned	intr	: 1;	/* Interrupt when done? */
	    volatile unsigned	last	: 1;	/* 1==Last descriptor in chain */
	    volatile unsigned	resv1	: 6;	/* Future use */
	    volatile unsigned	count	: 13;	/* Xfer size (bytes) */
	} bits;
    } dma_ctl;
    volatile unsigned long   gio_base;	/* Base GIO address */
    volatile unsigned long   next;	/* Physical address of next descriptor */
} VdeDescriptor;

/*
 * struct vmax
 * This structure represents the VMAX control registers.
 */
typedef struct vmax
{
    volatile unsigned long   id;	/* 0x00 GIO slot 0 id register. */
    volatile unsigned long   start;	/* 0x04 DMA start / stop control. */
    volatile unsigned long   reset;	/* 0x08 Intr, DMA, VMAX reset bits */
    volatile unsigned long   pad1;
    volatile unsigned long   desc_ptr;	/* 0x10 Pointer to beginning of descriptor chain. */
    volatile unsigned long   status;	/* 0x14 Status */
    volatile unsigned long   error_base;/* 0x18 Base of block which got BERR (VME addr) */
    volatile unsigned long   error_off;	/* 0x1c Remaining byte count when BERR occurred */
} Vmax;

/*
 * Definitions related to VMAX struct.
 */
#define	VMAX_STAT_MASK		(0xffff)
#define VMAX_STAT_XDONE		(1<<15)		/* 0x8000 */
#define VMAX_STAT_DDONE		(1<<14)		/* 0x4000 */
#define VMAX_STAT_IRESET	(1<<13)		/* 0x2000 */
#define VMAX_STAT_VRESET	(1<<12)		/* 0x1000 */
#define VMAX_STAT_DRESET	(1<<11)		/* 0x0800 */
#define VMAX_STAT_BERR		(1<<10)		/* 0x0400 */
#define VMAX_STAT_DMA_ST	(1<<9)		/* 0x0200 */
#define VMAX_STAT_A32_ID	(0xf<<5)	/* 0x01e0 */
#define VMAX_STAT_A32_EN	(1<<4)		/* 0x0010 */
#define VMAX_STAT_A24_ID	(3<<2)		/* 0x000c */
#define VMAX_STAT_A24_EN	(3<<0)		/* 0x0003 */

#define VMAX_STAT(bd)	((bd)->status & VMAX_STAT_MASK)

#define	VMAX_RESET_INT	(0x4)
#define	VMAX_RESET_VMAX	(0x2)
#define	VMAX_RESET_DMA	(0x1)


/*
 * Definitions for kernel access to the VMAX board.
 * (The base address is just the base of GIO slot 0 memory.)
 */
#define VMAX_BASE	(0x1f400000)
#define	K1_VMAX		((Vmax *)PHYS_TO_K1(VMAX_BASE))
#define	VMAX_IDMASK	(0xff)
#define	VMAX_ID		(0x03)
#define	BD_ID(b)	((b)->id & VMAX_IDMASK)
#define	IS_VMAX(b)	(BD_ID(b) == VMAX_ID)

#define	VMAX_PRESENT()	((VMAX_GET_ID() & VMAX_IDMASK) == VMAX_ID)

#ifndef	MIN
#define	MIN(x, y)	(((x) < (y)) ? (x) : (y))
#define	MAX(x, y)	(((x) >= (y)) ? (x) : (y))
#endif
