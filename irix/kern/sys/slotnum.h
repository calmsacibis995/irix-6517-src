/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1997, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef __SYS_SLOTNUM_H__
#define __SYS_SLOTNUM_H__

/*
 * This file attempts to define a slot number space across all slots.
 *
 *	Node slots
 *	Router slots
 *	Crosstalk slots
 *
 *	Other slots are children of their parent crosstalk slot:
 *		PCI slots
 *		VME slots
 */

#define SLOTNUM_NODE_CLASS	0x00	/* Node   */
#define SLOTNUM_ROUTER_CLASS	0x10	/* Router */
#define SLOTNUM_XTALK_CLASS	0x20	/* Xtalk  */
#define SLOTNUM_MIDPLANE_CLASS	0x30	/* Midplane */
#define SLOTNUM_XBOW_CLASS	0x40	/* Xbow  */
#define SLOTNUM_KNODE_CLASS	0x50	/* Kego node */
#define SLOTNUM_INVALID_CLASS	0xf0	/* Invalid */

#define SLOTNUM_CLASS_MASK	0xf0
#define SLOTNUM_SLOT_MASK	0x0f

#define SLOTNUM_GETCLASS(_sn)	((_sn) & SLOTNUM_CLASS_MASK)
#define SLOTNUM_GETSLOT(_sn)	((_sn) & SLOTNUM_SLOT_MASK)

#endif /* __SYS_SLOTNUM_H__ */

