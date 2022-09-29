/**************************************************************************
 *									  *
 * 		 Copyright (C) 1990,1992 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef __SYS_EVEREST_ADDRS_H__
#define __SYS_EVEREST_ADDRS_H__

#ident "$Revision: 1.11 $"

#include <sys/mips_addrspace.h>

/*
 * Definitions of physical memory usage for EVEREST
 * 
 * Physical memory usage for the EVEREST
 * 
 *
 *
 *				+-------------------------------+
 *				| IP21 PROM text and r/o data   |
 * 0x1fc00000			+-------------------------------+
 *				|				|
 *				    (Free space)	
 *				|				|
 * 0x00a00000 (10.0M)		+-------------------------------+
 *				| IO4/EPC enet rcv/xmt buffers  | 
 * 0x00900000 (9.0M)		+-------------------------------+
 *				| IO4 PROM text and data	|
 * 0x00800000 (8.0M)		+-------------------------------+
 *				| Grinch/Venice gfx PROM (copy) |
 * 0x00780000 (7.5M)		+-------------------------------+
 *				| PROM stack (1 per system) V	|
 *			        |				|	
 * 				| PROM BSS			|
 * 0x00680000 (6.5M)		+-------------------------------+
 * 				| Sash Text/Data/BSS		|
 * 0x00600000 (6M)		+-------------------------------+
 *				| Sash Stack (Grows down)	|
 *				|				|
 *				|				|
 *				| UNIX (debug version)		|
 * 0x00100000 (1M)		+-------------------------------+
 *				| Symmon (text/data/bss)	|
 * 0x00023000 (176K)		+-------------------------------+
 *				| Symmon stack for CPU #39	|
 * 0x0002c000 (172K)		+-------------------------------+
 *				|             ..		|
 *				|	      ..		|
 *				|             ..		|
 *				+-------------------------------+
 *				| Symmon stack for CPU #1	|
 * 0x00008000 (20K)		+-------------------------------+ 
 *				| Symmon stack for CPU #0	|
 *				|   (if debugging)		|
 *				|       ---------------		|
 *				| UNIX (non-debugging)		| 
 * 0x00006000 (24K)		+-------------------------------+
 *				| Write Gatherer test area	|
 * 0x00004000 (16K)		+-------------------------------+
 *				| Everest MPCONF block		|
 * 0x00003000 (12K)		+-------------------------------+
 *				| Ev sysconfig block 		|
 * 0x00002000 (8K)		+-------------------------------+
 *				| ARCS System Param Block 	|
 * 0x00001000 (4K)		+-------------------------------+
 *				| Cache err EFRAME	   	|
 * 0x00000e00 (3.5k)		+-------------------------------+
 *				| Everest extended error	|
 * 0x00000a00 (2.5K)		+-------------------------------+
 *				| Restart Block (1 per system)	|
 * 0x00000800 (2K)		+-------------------------------+
 *				| PROM/kernel Global Data Area  |
 * 0x00000400 (1K)		+-------------------------------+
 *				| General Exception Handler     |
 * 0x00000180			+-------------------------------+
 *				| Xutlbmiss handler 		|
 * 0x00000080			+-------------------------------+
 *				| utlbmiss handler		|
 * 0x00000000 (0K)		+-------------------------------+
 *
 * General Notes:  This memory map supports a maximum of 40 CPUS
 * (limited by number of symmon stacks).  Given the organization
 * of the system bus, there probably isn't any way to squeeze more
 * processors onto a backplane, so forty should be ample. 
 */


/*
 * Address definitions
 */
#define PHYS_RAMBASE		0x00000000
#define K0_RAMBASE		PHYS_TO_K0(PHYS_RAMBASE)

#define	GDA_ADDR		PHYS_TO_K1(0x00000400)
#define SYSPARAM_ADDR		PHYS_TO_K1(0x00001000)
#define EVCFGINFO_ADDR		PHYS_TO_K1(0x00002000)
#define EVERROR_ADDR		PHYS_TO_K1(0x00002800)
#define EVERROR_EXT_ADDR		PHYS_TO_K1(0x00000a00)
#define MPCONF_ADDR		PHYS_TO_K1(0x00003000)
#define WGTESTMEM_ADDR		PHYS_TO_K1(0x00003e00)
#define SYMMON_STACK_SIZE	0x2000
#define SYMMON_STACK_ADDR(_x)	(PHYS_TO_K0(0x00006000) + \
						SYMMON_STACK_SIZE * (_x))  
#define SYMMON_STACK		SYMMON_STACK_ADDR(0)
#define EARLY_ARCS_STACK	PHYS_TO_K0(0x00007000) /* overlaps symmon stk */
#define NODEBUGUNIX_ADDR	PHYS_TO_K0(0x00006000)
#define FREEMEM_BASE		PHYS_TO_K0(0x02000000)

/*
 * IP21 PROM vectors
 */

#define IP21PROM_RESTART	PHYS_TO_K1(0x1fc00008)
#define IP21PROM_RESLAVE	PHYS_TO_K1(0x1fc00010)
#define IP21PROM_PODMODE	PHYS_TO_K1(0x1fc00018)
#define IP21PROM_EPCUARTPOD	PHYS_TO_K1(0x1fc00020)
#define IP21PROM_FLASHLEDS	PHYS_TO_K1(0x1fc00028)

/*
 * EVEREST IO4 PROM addresses
 */
#ifdef SABLE
#define IP21PROM_STACK		PHYS_TO_K0(0x00780000)
#else
#define IP21PROM_STACK		PHYS_TO_K0(0x00980000)
#endif
#define IP21PROM_BASE		PHYS_TO_K1(0x1fc00000)
#define IP21PROM_SIZE		0x40000

#define PROM_BASE		PHYS_TO_K0(0x01800000)
#define PROM_SIZE		0x300000
#define DPROM_BASE		PHYS_TO_K0(0x01c00000)
#define DPROM_SIZE		0x400000
#define GFXPROM_BASE		PHYS_TO_K0(0x01700000)
#define GFXPROM_SIZE		0x100000
#define	ENETBUFS_BASE		0x01b00000	/* physical */
#define ENETBUFS_SIZE		0x100000
#define FLASHBUF_BASE		PHYS_TO_K0(0x01600000)
#define FLASHBUF_SIZE		0x100000
#define SLAVESTACK_BASE		PHYS_TO_K0(0x01580000)
#define SLAVESTACK_SIZE		0x40000
#define IO4STACK_SIZE		(128 * 1024)

#define EV_PROM_RESTART		IP21PROM_RESTART
#define EV_PROM_RESLAVE		IP21PROM_RESLAVE
#define EV_PROM_PODMODE		IP21PROM_PODMODE
#define EV_PROM_EPCUARTPOD	IP21PROM_EPCUARTPOD
#define EV_PROM_FLASHLEDS	IP21PROM_FLASHLEDS

#endif	/* __SYS_EVEREST_ADDRS_H__ */
