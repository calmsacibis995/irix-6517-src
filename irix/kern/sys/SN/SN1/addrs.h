/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1996-1998, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef __SYS_SN_SN1_ADDRS_H__
#define __SYS_SN_SN1_ADDRS_H__


/*
 * SN1 (on a Beast) Address map
 *
 * This file contains a set of definitions and macros which are used
 * to reference into the major address spaces (CAC, HSPEC, IO, MSPEC,
 * and UNCAC) used by the SN1 architecture.  It also contains addresses
 * for "major" statically locatable PROM/Kernel data structures, such as
 * the partition table, the configuration data structure, etc.
 * We make an implicit assumption that the processor using this file
 * follows the Beast's provisions for specifying uncached attributes;
 * should this change, the base registers may very well become processor-
 * dependent.
 *
 * For more information on the address spaces, see the "Local Resources"
 * chapter of the SNAC specification.
 *
 * NOTE: This header file is included both by C and by assembler source
 *	 files.  Please bracket any language-dependent definitions
 *	 appropriately.
 */


#define NODE_SIZE_BITS		33
#define BWIN_SIZE_BITS		29

#define NASID_BITMASK		(0x3FFLL)
#define NASID_BITS		11
#define NASID_SHFT		NODE_SIZE_BITS

#define NASID_META_BITS		4
#define NASID_LOCAL_BITS	4
#define BDDIR_UPPER_MASK	(UINT64_CAST 0x1fffff << 10)
#define BDECC_UPPER_MASK	(UINT64_CAST 0xfffffff << 3)

#define NODE_ADDRSPACE_SIZE	(UINT64_CAST 1 << NODE_SIZE_BITS)
#define NASID_MASK		(UINT64_CAST NASID_BITMASK << NASID_SHFT)
#define NASID_GET(_pa)		((PS_UINT_CAST (_pa) >> NODE_SIZE_BITS) & NASID_BITMASK)
/*
 * physical address bits 47 thru 44 define the attributes.
 */
#define ATTR_BITS		4
#define ATTR_SHFT		(NODE_SIZE_BITS + NASID_BITS)
#define ATTR_MASK		(UINT64_CAST 0xf << ATTR_SHFT)
#define ATTR_ADDRSPACE_SIZE	(UINT64_CAST 1 << ATTR_SHFT)
#define ATTR_SPACE(spc)		(ATTR_ADDRSPACE_SIZE * (spc))


/*
 * external agent attributes:
 */
#define CAC_ATTR                0
#define IO_ATTR			1
#define MSPEC_ATTR		2
#define UNCAC_ATTR		3
#define ASPEC_ATTR		4
#define ESPEC_ATTR		5
#define HSPEC_ATTR		7

#define CACHED_BASE		0xA800000000000000

#define UNCACHED_BASE		0x9000000000000000

#define CAC_BASE		(CACHED_BASE   | ATTR_SPACE(CAC_ATTR))
                                                /*  0xa800000000000000 */
#define IO_BASE			(UNCACHED_BASE | ATTR_SPACE(IO_ATTR))
                                                /*  0x9000100000000000 */
#define MSPEC_BASE		(UNCACHED_BASE | ATTR_SPACE(MSPEC_ATTR))
                                                /*  0x9000200000000000 */
#define UNCAC_BASE		(UNCACHED_BASE | ATTR_SPACE(UNCAC_ATTR))
                                                /*  0x9000300000000000 */
#define ASPEC_BASE		(CACHED_BASE   | ATTR_SPACE(ASPEC_ATTR))
                                                /*  0xa800400000000000 */
#define ESPEC_BASE		(CACHED_BASE   | ATTR_SPACE(ESPEC_ATTR))
                                                /*  0xa800500000000000 */
#define HSPEC_BASE		(UNCACHED_BASE | ATTR_SPACE(HSPEC_ATTR))
                                                /*  0x9000700000000000 */

#define TO_PHYS(x)		(	      ((x) & TO_PHYS_MASK))
#define TO_CAC(x)		(CAC_BASE   | ((x) & TO_PHYS_MASK))
#define TO_UNCAC(x)		(UNCAC_BASE | ((x) & TO_PHYS_MASK))
#define TO_MSPEC(x)		(MSPEC_BASE | ((x) & TO_PHYS_MASK))
#define TO_HSPEC(x)		(HSPEC_BASE | ((x) & TO_PHYS_MASK))
#define TO_ASPEC(x)		(ASPEC_BASE | ((x) & TO_PHYS_MASK))
#define TO_ESPEC(x)		(ESPEC_BASE | ((x) & TO_PHYS_MASK))
#define TO_IO(x)		(IO_BASE    | ((x) & TO_PHYS_MASK))

/*
 * The following definitions pertain to the IO special address
 * space.  They define the location of the big and little windows
 * of any given node.
 */
#define NODE_SWIN_BASE(nasid, widget) 					\
        (NODE_IO_BASE(nasid) + (PS_UINT_CAST (widget) << SWIN_SIZE_BITS))


#define BWIN_INDEX_BITS		3
#define BWIN_SIZE		(UINT64_CAST 1 << BWIN_SIZE_BITS)
#define	BWIN_SIZEMASK		(BWIN_SIZE - 1)
#define	BWIN_WIDGET_MASK	0x7
#define NODE_BWIN_BASE0(nasid)	(NODE_IO_BASE(nasid) + BWIN_SIZE)
#define NODE_BWIN_BASE(nasid, bigwin)	(NODE_BWIN_BASE0(nasid) + \
			(PS_UINT_CAST (bigwin) << BWIN_SIZE_BITS))

#define	BWIN_WIDGETADDR(addr)	((addr) & BWIN_SIZEMASK)
#define	BWIN_WINDOWNUM(addr)	(((addr) >> BWIN_SIZE_BITS) & BWIN_WIDGET_MASK)
/*
 * Verify if addr belongs to large window address of node with "nasid"
 *
 *
 * NOTE: "addr" is expected to be XKPHYS address, and NOT physical
 * address
 *
 *
 */

#if _LANGUAGE_C
#define	NODE_BWIN_ADDR(nasid, addr)	\
		(((addr) >= NODE_BWIN_BASE0(nasid)) && \
		 ((addr) < (NODE_BWIN_BASE(nasid, HUB_NUM_BIG_WINDOW) + \
				BWIN_SIZE)))

#endif /* _LANGUAGE_C */


/*
 * find out what we need and keep them. Throw away the rest.
 */
#ifdef NOTUSED
#ifdef PROM

#define MISC_PROM_BASE		PHYS_TO_K0(0x01300000)
#define MISC_PROM_SIZE		0x200000

#define DIAG_BASE		PHYS_TO_K0(0x01500000)
#define DIAG_SIZE		0x300000

#define GRAPHICS_PROM_BASE	PHYS_TO_K0(0x01800000)
#define GRAPHICS_PROM_SIZE	0x200000

#define IP27PROM_FLASH_HDR	PHYS_TO_K0(0x01300000)
#define IP27PROM_FLASH_DATA	PHYS_TO_K0(0x01301000)
#define IP27PROM_CORP_MAX	120
#define IP27PROM_CORP		PHYS_TO_K0(0x01800000)
#define IP27PROM_CORP_SIZE	0x10000
#define IP27PROM_CORP_STK	PHYS_TO_K0(0x01810000)
#define IP27PROM_CORP_STKSIZE	0x2000
#define IP27PROM_DECOMP_BUF	PHYS_TO_K0(0x01900000)
#define IP27PROM_DECOMP_SIZE	0xfff00

#define IP27PROM_BASE		PHYS_TO_K0(0x01a00000)
#define IP27PROM_BASE_MAPPED	(K2BASE | 0x1fc00000)
#define IP27PROM_SIZE_MAX	0x100000

#define IP27PROM_PCFG		PHYS_TO_K0(0x01b00000)
#define IP27PROM_PCFG_SIZE	0xd0000
#define IP27PROM_ERRDMP		PHYS_TO_K1(0x01bd0000)
#define IP27PROM_ERRDMP_SIZE	0xf000

#define IP27PROM_INIT_START	PHYS_TO_K1(0x01bd0000)
#define IP27PROM_CONSOLE	PHYS_TO_K1(0x01bdf000)
#define IP27PROM_CONSOLE_SIZE	0x200
#define IP27PROM_NETUART	PHYS_TO_K1(0x01bdf200)
#define IP27PROM_NETUART_SIZE	0x100
#define IP27PROM_UNUSED1	PHYS_TO_K1(0x01bdf300)
#define IP27PROM_UNUSED1_SIZE	0x500
#define IP27PROM_ELSC_BASE_A	PHYS_TO_K0(0x01bdf800)
#define IP27PROM_ELSC_BASE_B	PHYS_TO_K0(0x01bdfc00)
#define IP27PROM_STACK_A	PHYS_TO_K0(0x01be0000)
#define IP27PROM_STACK_B	PHYS_TO_K0(0x01bf0000)
#define IP27PROM_STACK_SHFT	16
#define IP27PROM_STACK_SIZE	(1 << IP27PROM_STACK_SHFT)
#define IP27PROM_INIT_END	PHYS_TO_K0(0x01c00000)

#define SLAVESTACK_BASE		PHYS_TO_K0(0x01580000)
#define SLAVESTACK_SIZE		0x40000

#define ENETBUFS_BASE		PHYS_TO_K0(0x01f80000)
#define ENETBUFS_SIZE		0x20000

#define IO6PROM_BASE		PHYS_TO_K0(0x01c00000)
#define IO6PROM_SIZE		0x300000
#define	IO6PROM_BASE_MAPPED	(K2BASE | 0x11c00000)
#define IO6DPROM_BASE		PHYS_TO_K0(0x01c00000)
#define IO6DPROM_SIZE		0x200000

#define NODEBUGUNIX_ADDR	PHYS_TO_K0(0x00019000)
#define DEBUGUNIX_ADDR		PHYS_TO_K0(0x00100000)

#define IP27PROM_INT_LAUNCH	10	/* and 11 */
#define IP27PROM_INT_NETUART	12	/* through 17 */

#endif /* PROM */

/*
 * needed by symmon so it needs to be outside #if PROM
 */
#define IP27PROM_ELSC_SHFT	10
#define IP27PROM_ELSC_SIZE	(1 << IP27PROM_ELSC_SHFT)

/*
 * This address is used by IO6PROM to build MemoryDescriptors of
 * free memory. This address is important since unix gets loaded
 * at this address, and this memory has to be FREE if unix is to
 * be loaded.
 */

#define FREEMEM_BASE		PHYS_TO_K0(0x2000000)

#define IO6PROM_STACK_SHFT	14	/* stack per cpu */
#define IO6PROM_STACK_SIZE	(1 << IO6PROM_STACK_SHFT)

/*
 * IP27 PROM vectors
 */

#define IP27PROM_ENTRY		PHYS_TO_COMPATK1(0x1fc00000)
#define IP27PROM_RESTART	PHYS_TO_COMPATK1(0x1fc00008)
#define IP27PROM_SLAVELOOP	PHYS_TO_COMPATK1(0x1fc00010)
#define IP27PROM_PODMODE	PHYS_TO_COMPATK1(0x1fc00018)
#define IP27PROM_IOC3UARTPOD	PHYS_TO_COMPATK1(0x1fc00020)
#define IP27PROM_FLASHLEDS	PHYS_TO_COMPATK1(0x1fc00028)
#define IP27PROM_REPOD		PHYS_TO_COMPATK1(0x1fc00030)
#define IP27PROM_LAUNCHSLAVE	PHYS_TO_COMPATK1(0x1fc00038)
#define IP27PROM_WAITSLAVE	PHYS_TO_COMPATK1(0x1fc00040)
#define IP27PROM_POLLSLAVE	PHYS_TO_COMPATK1(0x1fc00048)

#define KL_UART_BASE	LOCAL_HUB_ADDR(MD_UREG0_0)	/* base of UART regs */
#define KL_UART_CMD	LOCAL_HUB_ADDR(MD_UREG0_0)	/* UART command reg */
#define KL_UART_DATA	LOCAL_HUB_ADDR(MD_UREG0_1)	/* UART data reg */
#define KL_I2C_REG	MD_UREG0_0			/* I2C reg */

#define _ARCSPROM

#ifdef _STANDALONE

/*
 * The PROM needs to pass the device base address and the
 * device pci cfg space address to the device drivers during
 * install. The COMPONENT->Key field is used for this purpose.
 * Macros needed by SN0 device drivers to convert the
 * COMPONENT->Key field to the respective base address.
 * Key field looks as follows:
 *
 *  +----------------------------------------------------+
 *  |devnasid | widget  |pciid |hubwidid|hstnasid | adap |
 *  |   2     |   1     |  1   |   1    |    2    |   1  |
 *  +----------------------------------------------------+
 *  |         |         |      |        |         |      |
 *  64        48        40     32       24        8      0
 *
 * These are used by standalone drivers till the io infrastructure
 * is in place.
 */

#if _LANGUAGE_C

#define uchar unsigned char

#define KEY_DEVNASID_SHFT  48
#define KEY_WIDID_SHFT	   40
#define KEY_PCIID_SHFT	   32
#define KEY_HUBWID_SHFT	   24
#define KEY_HSTNASID_SHFT  8

#define MK_SN0_KEY(nasid, widid, pciid) \
			((((__psunsigned_t)nasid)<< KEY_DEVNASID_SHFT |\
				((__psunsigned_t)widid) << KEY_WIDID_SHFT) |\
				((__psunsigned_t)pciid) << KEY_PCIID_SHFT)

#define ADD_HUBWID_KEY(key,hubwid)\
			(key|=((__psunsigned_t)hubwid << KEY_HUBWID_SHFT))

#define ADD_HSTNASID_KEY(key,hstnasid)\
			(key|=((__psunsigned_t)hstnasid << KEY_HSTNASID_SHFT))

#define GET_DEVNASID_FROM_KEY(key)	((short)(key >> KEY_DEVNASID_SHFT))
#define GET_WIDID_FROM_KEY(key)		((uchar)(key >> KEY_WIDID_SHFT))
#define GET_PCIID_FROM_KEY(key)		((uchar)(key >> KEY_PCIID_SHFT))
#define GET_HUBWID_FROM_KEY(key)	((uchar)(key >> KEY_HUBWID_SHFT))
#define GET_HSTNASID_FROM_KEY(key)	((short)(key >> KEY_HSTNASID_SHFT))

#define PCI_64_TARGID_SHFT		60

#define GET_PCIBASE_FROM_KEY(key)  (NODE_SWIN_BASE(GET_DEVNASID_FROM_KEY(key),\
					GET_WIDID_FROM_KEY(key))\
					| BRIDGE_DEVIO(GET_PCIID_FROM_KEY(key)))

#define GET_PCICFGBASE_FROM_KEY(key) \
			(NODE_SWIN_BASE(GET_DEVNASID_FROM_KEY(key),\
			      GET_WIDID_FROM_KEY(key))\
			| BRIDGE_TYPE0_CFG_DEV(GET_PCIID_FROM_KEY(key)))

#define GET_WIDBASE_FROM_KEY(key) \
                        (NODE_SWIN_BASE(GET_DEVNASID_FROM_KEY(key),\
                              GET_WIDID_FROM_KEY(key)))

#define PUT_INSTALL_STATUS(c,s)		c->Revision = s
#define GET_INSTALL_STATUS(c)		c->Revision

#endif /* LANGUAGE_C */

#endif /* _STANDALONE */

#endif /* NOTUSED */

#define IP33PROM_LAUNCHSLAVE	NULL
#define IP33PROM_WAITSLAVE	NULL
#define IP33PROM_POLLSLAVE	NULL

#endif /* __SYS_SN_SN1_ADDRS_H__ */
