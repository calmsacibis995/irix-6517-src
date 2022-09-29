/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1990, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident "sys/giobus.h: $Revision: 1.5 $"

#ifndef	_SYS_GIOBUS_H
#define	_SYS_GIOBUS_H
/*
 * GIObus definitions defined in IP12.h/IP20.h/IP22.h are copied to this
 * file for Everest usage. 
 */

/* Local IO Segment */
#define LIO_ADDR        0x1f000000
#define LIO_GFX_SIZE    0x00400000      /* 4 Meg for graphics boards */
#define LIO_GIO_SIZE    0x00600000      /* 6 Meg for GIO Slots */

/* GIO Interrupts */
#define GIO_INTERRUPT_0         0
#define GIO_INTERRUPT_1         1
#define GIO_INTERRUPT_2         2

/* GIO Slots */
#define GIO_SLOT_0              0
#define GIO_SLOT_1              1
#define GIO_SLOT_GFX            2


/* K1 seg addresses mapping to these slots */
#if _MIPS_SIM == _ABI64
#define	GIO_SLOTGFX		0x900000001F000000
#define	GIO_SLOT0		0x900000001F400000
#define	GIO_SLOT1		0x900000001F600000
#else
#define	GIO_SLOTGFX		0xBF000000	/* Valid only for IP22 */
#define	GIO_SLOT0		0xBF400000
#define	GIO_SLOT1		0xBF600000
#endif

#define GIOLEVELS       3 /* GIO 0 (FIFO), GIO 1 (GE), GIO 2 (VR) */
#define GIOSLOTS        3 /* Graphics = 0, GIO 1 & 2 */


/* GIO config register Address */
#define GIO_SLOT0_ADDR	0x1fa20000	/* Slot 0 config register (w) */
#define GIO_SLOT1_ADDR	0x1fa20004	/* Slot 1 config register (w) */

/* GIO bus IDs
 *
 * Each GIO bus device identifies itself to the system by answering a
 * read with an "ID" value.  IDs are either 8 or 32 bits long.  IDs less
 * than 128 are 8 bits long, with the most significant 24 bits read from
 * the slot undefined.
 *
 * 32-bit IDs are currently divided into
 *	bits 0-6        the product ID; ranges from 0x00 to 0x7F.
 *	bit 7		0=GIO Product ID is 8 bits wide
 *			1=GIO Product ID is 32 bits wide.
 *	bits 15-8       manufacturer's version for the product.
 *	bit 16		0=GIO32 and GIO32-bis, 1=GIO64.
 *	bit 17		0=no ROM present
 *			1=ROM present on this board AND next three words
 *				space define the ROM.
 *	bits 31-18	up to manufacturer.
 * Note that this may change in future 32-bit IDs
 *
 * IDs above 0x50/0xd0 are for 3rd party boards.
 *
 * Contact the Developer Program to allocate new IDs, recently Rick McLeod.
 *
 * This list must match the list in system.gen.
 */

#define GIO_SID_MASK	0xff		/* only 8 bits valid for IDs<0x80 */
#define GIO_LID_MASK	0xff		/* only 8 bits of IDs for others */

/* Short, 8-bit IDs */
#define GIO_ID_XPI	0x01		/* XPI low cost FDDI */
#define GIO_ID_GTR	0x02		/* GTR TokenRing */
/*			0x04		   Synchronous ISDN */
/*			0x06		   Canon Interface */
/*			0x08		   JPEG (Double Wide) */
/*			0x09		   JPEG (Single Wide) */
#define GIO_ID_XPI_M0	0x0a		/* XPI mez. FDDI device 0 */
#define GIO_ID_XPI_M1	0x0b		/* XPI mez. FDDI device 1 */
#define GIO_ID_EP	0x0e		/* E-Plex 8-port Ethernet */
#define GIO_ID_IVAS	0x30		/* Lyon Lamb IVAS */

/* Long, 32-bit IDs */
#define GIO_ID_ATM	0x85		/* ATM board */
/*			0x87		   16 bit SCSI Card */
/*			0x8c		   SMPTE 259M Video */
/*			0x8d		   Babblefish Compression */

#ifdef _KERNEL
#include <sys/hwgraph.h>
graph_error_t	hpc_hwgraph_lookup(vertex_hdl_t *);
graph_error_t	gio_hwgraph_lookup(caddr_t, vertex_hdl_t *);
#endif
#endif /* _SYS_GIOBUS_H */

